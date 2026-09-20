// Treemap Disk Visualizer, iteration 2 (plan 20260920_02_redo).
//
// Serial reference scan + squarified layout (MVP logic), plus a parallel
// variant built only on std::thread::scope over owned values (no new
// dependency). `--scan <dir>` prints one line (BrokenPipe-safe),
// `--bench <dir>` compares serial vs parallel timings. Otherwise the GUI
// runs on the parallel path via Window::from_config.
use macroquad::prelude::*;
use std::ffi::OsStr;
use std::fs;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::mpsc::{Receiver, channel};
use std::thread;
use std::time::Instant;

struct Node {
    path: PathBuf,
    size: u64,
    is_dir: bool,
    children: Vec<Node>,
    rect: Rect,
    color: Color,
}

fn report_skip(context: &str, path: &Path, err: &std::io::Error) {
    eprintln!("skip [{context}]: {}: {err:?}", path.display());
}

fn is_virtual(path: &Path) -> bool {
    let s = path.to_string_lossy();
    s.starts_with("/proc") || s.starts_with("/sys") || s.starts_with("/dev")
}

fn dir_node(path: &Path) -> Node {
    Node {
        path: path.to_path_buf(),
        size: 0,
        is_dir: true,
        children: Vec::new(),
        rect: Rect::default(),
        color: Color::new(0.15, 0.17, 0.22, 1.0),
    }
}

// ----------------------------------------------------------------------------
// 1. Serial scan (reference)
// ----------------------------------------------------------------------------
fn scan_tree(path: &Path) -> Node {
    let mut node = dir_node(path);
    if is_virtual(path) {
        return node;
    }
    match fs::read_dir(path) {
        Ok(entries) => {
            for entry_res in entries {
                match entry_res {
                    Ok(entry) => scan_entry(&mut node, &entry),
                    Err(e) => report_skip("entry", path, &e),
                }
            }
        }
        Err(e) => report_skip("read_dir", path, &e),
    }
    node
}

fn scan_entry(node: &mut Node, entry: &std::fs::DirEntry) {
    let ft = match entry.file_type() {
        Ok(ft) => ft,
        Err(e) => {
            report_skip("file_type", &entry.path(), &e);
            return;
        }
    };
    if ft.is_symlink() {
        return;
    }
    let entry_path = entry.path();
    if ft.is_dir() {
        let child = scan_tree(&entry_path);
        if child.size > 0 {
            node.size += child.size;
            node.children.push(child);
        }
    } else if ft.is_file() {
        let size = match entry.metadata() {
            Ok(m) => m.len(),
            Err(e) => {
                report_skip("metadata", &entry_path, &e);
                0
            }
        };
        if let Some(child) = file_node(entry_path, size) {
            node.size += child.size;
            node.children.push(child);
        }
    }
}

fn file_node(entry_path: PathBuf, size: u64) -> Option<Node> {
    if size > 0 && size < (1 << 48) {
        Some(Node {
            color: color_for_path(&entry_path),
            path: entry_path,
            size,
            is_dir: false,
            children: Vec::new(),
            rect: Rect::default(),
        })
    } else {
        None
    }
}

// ----------------------------------------------------------------------------
// 2. Parallel scan (std::thread::scope over owned values)
// ----------------------------------------------------------------------------
static SCAN_ACTIVE: AtomicUsize = AtomicUsize::new(0);

fn worker_limit() -> usize {
    4 * std::thread::available_parallelism()
        .map(|n| n.get())
        .unwrap_or(2)
        .max(2)
}

fn scan_tree_parallel(path: &Path) -> Node {
    scan_tree_parallel_impl(path, 0)
}

fn scan_tree_parallel_impl(path: &Path, depth: usize) -> Node {
    let mut node = dir_node(path);
    if is_virtual(path) {
        return node;
    }
    // Files are cheap: handle inline. Only subdirectories (few, deep work)
    // get their own scope thread. Thread-per-file was 12x slower (iter1).
    let mut subdirs: Vec<PathBuf> = Vec::new();
    match fs::read_dir(path) {
        Ok(rd) => {
            for entry_res in rd {
                let entry = match entry_res {
                    Ok(entry) => entry,
                    Err(e) => {
                        report_skip("entry", path, &e);
                        continue;
                    }
                };
                let ft = match entry.file_type() {
                    Ok(ft) => ft,
                    Err(e) => {
                        report_skip("file_type", &entry.path(), &e);
                        continue;
                    }
                };
                if ft.is_symlink() {
                    continue;
                }
                let entry_path = entry.path();
                if ft.is_dir() {
                    subdirs.push(entry_path);
                } else if ft.is_file() {
                    let size = match entry.metadata() {
                        Ok(m) => m.len(),
                        Err(e) => {
                            report_skip("metadata", &entry_path, &e);
                            0
                        }
                    };
                    if let Some(child) = file_node(entry_path, size) {
                        node.size += child.size;
                        node.children.push(child);
                    }
                }
            }
        }
        Err(e) => {
            report_skip("read_dir", path, &e);
            return node;
        }
    }

    // Capped fan-out from the start (iter1 lesson): uncapped spawning
    // drowned the gains on warm cache.
    let mut fanout = !subdirs.is_empty() && depth < 64;
    if fanout {
        let was = SCAN_ACTIVE.fetch_add(1, Ordering::SeqCst);
        fanout = was < worker_limit();
        if !fanout {
            SCAN_ACTIVE.fetch_sub(1, Ordering::SeqCst);
        }
    }
    if fanout {
        thread::scope(|s| {
            let mut handles = Vec::new();
            for sub in subdirs {
                handles.push(s.spawn(move || scan_tree_parallel_impl(&sub, depth + 1)));
            }
            for h in handles {
                match h.join() {
                    Ok(child) => {
                        if child.size > 0 {
                            node.size += child.size;
                            node.children.push(child);
                        }
                    }
                    Err(_) => eprintln!("scan thread panicked; subtree dropped"),
                }
            }
        });
        SCAN_ACTIVE.fetch_sub(1, Ordering::SeqCst);
    } else {
        for sub in subdirs {
            let child = scan_tree_parallel_impl(&sub, depth + 1);
            if child.size > 0 {
                node.size += child.size;
                node.children.push(child);
            }
        }
    }
    node
}

// ----------------------------------------------------------------------------
// 3. Squarified layout (serial reference + parallel driver)
// ----------------------------------------------------------------------------
fn worst(areas: &[f64], row: &[usize], sum: f64, side: f32) -> f64 {
    let (s2, sum2) = (f64::from(side * side), sum * sum);
    row.iter()
        .map(|&i| (s2 * areas[i] / sum2).max(sum2 / (s2 * areas[i])))
        .fold(0.0, f64::max)
}

fn layout_row(nodes: &mut [Node], areas: &[f64], row: &[usize], sum: f64, r: &mut Rect) {
    let side = r.w.min(r.h);
    let thickness = (sum / f64::from(side)) as f32;
    let is_horiz = r.w < r.h;
    let mut offset = if is_horiz { r.x } else { r.y };

    for &i in row {
        let item_len = (areas[i] / sum * f64::from(side)) as f32;
        nodes[i].rect = if is_horiz {
            Rect::new(offset, r.y, item_len, thickness)
        } else {
            Rect::new(r.x, offset, thickness, item_len)
        };
        offset += item_len;
    }

    if is_horiz {
        r.y += thickness;
        r.h -= thickness;
    } else {
        r.x += thickness;
        r.w -= thickness;
    }
}

fn squarify_level(nodes: &mut [Node], mut rect: Rect) {
    let total: u64 = nodes.iter().map(|n| n.size).sum();
    if total == 0 || rect.w <= 0.0 || rect.h <= 0.0 {
        return;
    }

    nodes.sort_unstable_by_key(|n| std::cmp::Reverse(n.size));
    let area_mult = (rect.w * rect.h) as f64 / total as f64;
    let areas: Vec<f64> = nodes.iter().map(|n| n.size as f64 * area_mult).collect();

    let mut row = Vec::new();
    let mut row_sum = 0.0;

    for (i, &area) in areas.iter().enumerate() {
        let side = rect.w.min(rect.h);
        let mut next_row = row.clone();
        next_row.push(i);

        if row.is_empty()
            || worst(&areas, &next_row, row_sum + area, side) <= worst(&areas, &row, row_sum, side)
        {
            row.push(i);
            row_sum += area;
        } else {
            layout_row(nodes, &areas, &row, row_sum, &mut rect);
            row = vec![i];
            row_sum = area;
        }
    }
    if !row.is_empty() {
        layout_row(nodes, &areas, &row, row_sum, &mut rect);
    }
}

fn squarify(nodes: &mut [Node], rect: Rect) {
    squarify_level(nodes, rect);
    for node in nodes.iter_mut() {
        if node.is_dir && node.rect.w > 4.0 && node.rect.h > 4.0 {
            squarify(&mut node.children, node.rect);
        }
    }
}

// Owned subtree in, laid-out subtree out. Deeper levels stay serial:
// unbounded fan-out at every level cost more than it saved (iter1).
fn layout_subtree_parallel(mut node: Node, depth: usize) -> Node {
    if node.is_dir && !node.children.is_empty() && node.rect.w > 4.0 && node.rect.h > 4.0 {
        squarify_parallel_depth(&mut node.children, node.rect, depth + 1);
    }
    node
}

fn squarify_parallel(nodes: &mut Vec<Node>, rect: Rect) {
    squarify_parallel_depth(nodes, rect, 0);
}

fn squarify_parallel_depth(nodes: &mut Vec<Node>, rect: Rect, depth: usize) {
    squarify_level(nodes, rect);
    if depth > 0 {
        // Serial recursion below the top level (see above).
        for node in nodes.iter_mut() {
            if node.is_dir && !node.children.is_empty() && node.rect.w > 4.0 && node.rect.h > 4.0 {
                squarify_parallel_depth(&mut node.children, node.rect, depth + 1);
            }
        }
        return;
    }
    thread::scope(|s| {
        let taken = std::mem::take(nodes);
        let mut handles = Vec::new();
        let mut out: Vec<Node> = Vec::new();
        // Leaves need no recursion: keep them inline, spawn only subtrees.
        for child in taken {
            if child.is_dir
                && !child.children.is_empty()
                && child.rect.w > 4.0
                && child.rect.h > 4.0
            {
                handles.push(s.spawn(move || layout_subtree_parallel(child, depth)));
            } else {
                out.push(child);
            }
        }
        for h in handles {
            match h.join() {
                Ok(child) => out.push(child),
                Err(_) => eprintln!("layout thread panicked; subtree dropped"),
            }
        }
        // Threads rejoin out of order: restore size order (rects travel along).
        out.sort_unstable_by_key(|n| std::cmp::Reverse(n.size));
        *nodes = out;
    });
}

// ----------------------------------------------------------------------------
// 4. Rendering & utilities
// ----------------------------------------------------------------------------
fn render_tree(node: &Node, mouse: Vec2, hovered: &mut Option<String>) {
    if node.rect.w < 1.0 || node.rect.h < 1.0 {
        return;
    }

    if node.children.is_empty() {
        draw_rectangle(
            node.rect.x,
            node.rect.y,
            node.rect.w,
            node.rect.h,
            node.color,
        );
        draw_rectangle_lines(
            node.rect.x,
            node.rect.y,
            node.rect.w,
            node.rect.h,
            1.0,
            Color::new(0., 0., 0., 0.35),
        );
    } else {
        for child in &node.children {
            render_tree(child, mouse, hovered);
        }
        draw_rectangle_lines(
            node.rect.x,
            node.rect.y,
            node.rect.w,
            node.rect.h,
            1.0,
            Color::new(0., 0., 0., 0.6),
        );
    }

    if hovered.is_none() && node.rect.contains(mouse) {
        *hovered = Some(format!(
            "{} ({})",
            node.path.display(),
            format_bytes(node.size)
        ));
    }
}

fn hash_color(path: &Path) -> Color {
    let name = path.file_name().and_then(OsStr::to_str).unwrap_or("");
    let h = name
        .bytes()
        .fold(0u32, |acc, b| acc.wrapping_add(u32::from(b)));
    Color::from_rgba(
        (h.wrapping_mul(37) % 160 + 80) as u8,
        (h.wrapping_mul(59) % 160 + 80) as u8,
        (h.wrapping_mul(83) % 160 + 80) as u8,
        255,
    )
}

fn color_for_path(path: &Path) -> Color {
    let ext = path.extension().and_then(OsStr::to_str).unwrap_or("");
    match ext {
        "rs" | "c" | "cpp" | "py" | "js" | "ts" | "txt" | "md" => Color::new(0.2, 0.75, 0.45, 1.0),
        "png" | "jpg" | "jpeg" | "svg" | "webp" => Color::new(0.2, 0.65, 0.95, 1.0),
        "mp4" | "mkv" | "mov" | "mp3" | "flac" => Color::new(0.75, 0.35, 0.85, 1.0),
        "zip" | "tar" | "gz" | "7z" => Color::new(0.9, 0.3, 0.25, 1.0),
        _ => hash_color(path),
    }
}

fn format_bytes(b: u64) -> String {
    const UNITS: [&str; 5] = ["B", "KB", "MB", "GB", "TB"];
    let (mut d, mut i) = (b as f64, 0);
    while d >= 1024.0 && i < UNITS.len() - 1 {
        d /= 1024.0;
        i += 1;
    }
    format!("{:.1} {}", d, UNITS[i])
}

// BrokenPipe-safe single-line printer (iter1 lesson).
fn print_line(line: String) {
    let stdout = std::io::stdout();
    let mut lock = stdout.lock();
    if let Err(e) = writeln!(lock, "{line}") {
        match e.kind() {
            std::io::ErrorKind::BrokenPipe => {}
            _ => panic!("failed printing to stdout: {e}"),
        }
    }
}

fn count_files(node: &Node) -> u64 {
    if node.children.is_empty() {
        u64::from(!node.is_dir)
    } else {
        node.children.iter().map(count_files).sum()
    }
}

// ----------------------------------------------------------------------------
// 5. CLI: --scan and --bench (headless, no window)
// ----------------------------------------------------------------------------
fn run_scan(target: &Path) -> i32 {
    let root = scan_tree_parallel(target);
    print_line(format!(
        "{}: {} in {} files",
        root.path.display(),
        format_bytes(root.size),
        count_files(&root)
    ));
    0
}

fn bench_path(target: &Path) {
    for round in 1..=5 {
        let t0 = Instant::now();
        let mut s = scan_tree(target);
        let t_scan_ser = t0.elapsed().as_secs_f64();
        let t1 = Instant::now();
        squarify(&mut s.children, Rect::new(0.0, 0.0, 1920.0, 1044.0));
        let t_layout_ser = t1.elapsed().as_secs_f64();

        let t2 = Instant::now();
        let mut p = scan_tree_parallel(target);
        let t_scan_par = t2.elapsed().as_secs_f64();
        let t3 = Instant::now();
        squarify_parallel(&mut p.children, Rect::new(0.0, 0.0, 1920.0, 1044.0));
        let t_layout_par = t3.elapsed().as_secs_f64();

        assert_eq!(s.size, p.size, "serial vs parallel size mismatch");
        eprintln!(
            "[bench] round {round}: serial scan={t_scan_ser:.3}s layout={t_layout_ser:.3}s | \
             parallel scan={t_scan_par:.3}s layout={t_layout_par:.3}s | \
             size={} scan_speedup={:.2}x",
            format_bytes(s.size),
            t_scan_ser / t_scan_par.max(1e-9),
        );
    }
}

// ----------------------------------------------------------------------------
// 6. GUI main loop (parallel scan in background thread)
// ----------------------------------------------------------------------------
fn window_conf() -> Conf {
    Conf {
        window_title: "Treemap Disk Visualizer iter2 (parallel)".to_string(),
        window_width: 1280,
        window_height: 720,
        ..Default::default()
    }
}

// Plain main: --bench/--scan run headless (no window). The GUI starts only
// via Window::from_config.
fn main() {
    let args: Vec<String> = std::env::args().collect();
    if args.iter().any(|a| a == "--bench") {
        let dir = args
            .iter()
            .skip_while(|a| *a != "--bench")
            .nth(1)
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from("."));
        bench_path(&dir);
        return;
    }
    if args.iter().any(|a| a == "--scan") {
        let dir = args
            .iter()
            .skip_while(|a| *a != "--scan")
            .nth(1)
            .map(PathBuf::from)
            .unwrap_or_else(|| PathBuf::from("."));
        std::process::exit(run_scan(&dir));
    }

    macroquad::Window::from_config(window_conf(), gui_main());
}

async fn gui_main() {
    let args: Vec<String> = std::env::args().collect();
    let raw_target = args
        .get(1)
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from("."));

    let target = fs::canonicalize(&raw_target).unwrap_or(raw_target);
    eprintln!("target [target]={:?}", target.display());
    let (tx, rx): (std::sync::mpsc::Sender<Node>, Receiver<Node>) = channel();

    thread::spawn(move || {
        let root = scan_tree_parallel(&target);
        let _ = tx.send(root);
    });

    let mut root: Option<Node> = None;
    let mut last_size = (0.0, 0.0);

    loop {
        clear_background(Color::new(0.08, 0.09, 0.12, 1.0));
        let (screen_w, screen_h) = (screen_width(), screen_height());

        if let Ok(loaded_root) = rx.try_recv() {
            eprintln!(
                "layout [screen_w]={:?} [screen_h]={:?} [size]={:?}",
                screen_w, screen_h, loaded_root.size
            );
            root = Some(loaded_root);
            last_size = (0.0, 0.0);
        }

        if let Some(tree) = &mut root {
            let canvas = Rect::new(0.0, 36.0, screen_w, (screen_h - 36.0).max(1.0));

            if (screen_w, screen_h) != last_size {
                tree.rect = canvas;
                squarify_parallel(&mut tree.children, canvas);
                last_size = (screen_w, screen_h);
            }

            let mut hovered = None;
            render_tree(tree, Vec2::from(mouse_position()), &mut hovered);

            draw_rectangle(0.0, 0.0, screen_w, 36.0, Color::new(0.05, 0.06, 0.08, 0.95));
            let header = hovered
                .unwrap_or_else(|| format!("{}: {}", tree.path.display(), format_bytes(tree.size)));
            draw_text(&header, 14.0, 24.0, 16.0, WHITE);
        } else {
            draw_text("Scanning filesystem...", 20.0, 40.0, 24.0, LIGHTGRAY);
        }

        next_frame().await;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn format_bytes_units() {
        assert_eq!(format_bytes(0), "0.0 B");
        assert_eq!(format_bytes(100), "100.0 B");
        assert_eq!(format_bytes(2048), "2.0 KB");
        assert_eq!(format_bytes(5_242_880), "5.0 MB");
    }

    fn fixture(name: &str) -> PathBuf {
        let base = std::env::temp_dir().join(name);
        let _ = fs::remove_dir_all(&base);
        fs::create_dir_all(base.join("sub")).unwrap();
        fs::write(base.join("a.bin"), vec![0u8; 100]).unwrap();
        fs::write(base.join("sub").join("b.bin"), vec![0u8; 200]).unwrap();
        fs::write(base.join("empty"), Vec::new()).unwrap();
        base
    }

    #[test]
    fn serial_parallel_scan_equivalence() {
        let base = fixture("treemap_iter2_eq");
        let s = scan_tree(&base);
        let p = scan_tree_parallel(&base);
        assert_eq!(s.size, 300);
        assert_eq!(p.size, 300);
        assert_eq!(s.children.len(), p.children.len());
        let _ = fs::remove_dir_all(&base);
    }

    #[test]
    fn serial_parallel_layout_invariants() {
        let base = fixture("treemap_iter2_layout");
        let canvas = Rect::new(0.0, 0.0, 800.0, 600.0);
        let mut s = scan_tree(&base);
        let mut p = scan_tree_parallel(&base);
        squarify(&mut s.children, canvas);
        squarify_parallel(&mut p.children, canvas);
        for root in [&s, &p] {
            let area: f32 = root.children.iter().map(|n| n.rect.w * n.rect.h).sum();
            let total = canvas.w * canvas.h;
            assert!((area - total).abs() / total < 0.005, "area not preserved");
            let sizes: Vec<u64> = root.children.iter().map(|n| n.size).collect();
            let mut sorted = sizes.clone();
            sorted.sort_unstable_by_key(|v| std::cmp::Reverse(*v));
            assert_eq!(sizes, sorted, "children not size-sorted");
        }
        let _ = fs::remove_dir_all(&base);
    }

    #[test]
    fn scan_bench_compare() {
        let base = fixture("treemap_iter2_bench");
        let t0 = Instant::now();
        let s = scan_tree(&base);
        let t_ser = t0.elapsed().as_secs_f64();
        let t1 = Instant::now();
        let p = scan_tree_parallel(&base);
        let t_par = t1.elapsed().as_secs_f64();
        assert_eq!(s.size, p.size);
        eprintln!(
            "[bench-test] serial={t_ser:.4}s parallel={t_par:.4}s size={}",
            s.size
        );
        let _ = fs::remove_dir_all(&base);
    }
}
