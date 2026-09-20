// Black-box tests: run the built binary with --scan (no window needed).
use std::fs;
use std::path::PathBuf;
use std::process::{Command, Stdio};

fn bin() -> PathBuf {
    let mut p = std::env::current_exe().unwrap();
    p.pop();
    if p.ends_with("deps") {
        p.pop();
    }
    p.join("treemap_iter2")
}

fn fixture(name: &str) -> PathBuf {
    let base = std::env::temp_dir().join(name);
    let _ = fs::remove_dir_all(&base);
    fs::create_dir_all(base.join("sub")).unwrap();
    fs::write(base.join("a.bin"), vec![0u8; 100]).unwrap();
    fs::write(base.join("sub").join("b.bin"), vec![0u8; 200]).unwrap();
    base
}

#[test]
fn headless_scan_reports_size() {
    let base = fixture("treemap_iter2_scan");
    let out = Command::new(bin())
        .args(["--scan", &base.to_string_lossy()])
        .output()
        .expect("run --scan");
    assert!(out.status.success());
    let stdout = String::from_utf8_lossy(&out.stdout);
    assert!(stdout.contains("300.0 B"), "unexpected stdout: {stdout}");
    assert!(stdout.contains("2 files"), "unexpected stdout: {stdout}");
    let _ = fs::remove_dir_all(&base);
}

#[test]
fn headless_survives_closed_stdout() {
    let base = fixture("treemap_iter2_pipe");
    // `| head -n 1`: the reader goes away; exit must stay 0, no panic.
    let mut producer = Command::new(bin())
        .args(["--scan", &base.to_string_lossy()])
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .expect("spawn --scan");
    let mut head = Command::new("head")
        .arg("-n")
        .arg("1")
        .stdin(producer.stdout.take().unwrap())
        .stdout(Stdio::null())
        .spawn()
        .expect("spawn head");
    let head_status = head.wait().expect("wait head");
    let _ = producer.wait();
    assert!(head_status.success());
    let _ = fs::remove_dir_all(&base);
}

#[test]
fn headless_missing_dir_no_panic() {
    let out = Command::new(bin())
        .args(["--scan", "/nonexistent-treemap-dir-xyz"])
        .output()
        .expect("run --scan missing");
    assert!(out.status.success());
    let stderr = String::from_utf8_lossy(&out.stderr);
    assert!(stderr.contains("skip"), "expected skip report: {stderr}");
    assert!(!stderr.contains("panicked"), "must not panic");
}
