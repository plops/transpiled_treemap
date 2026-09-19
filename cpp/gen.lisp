;; gen.lisp --- cl-cpp-generator2 source of truth for the C++ treemap core.
;;
;; Run from the repo root:
;;   sbcl --load cpp/gen.lisp --quit
;; Writes:
;;   cpp/gen/010_scanner.hpp  cpp/gen/015_color.hpp
;;   cpp/gen/020_layout.hpp   (030_pge_app.hpp stays handwritten: PGE3 only)
;;
;; Pattern: cl-cpp-generator2/example/09_concurrent_producer_fsm/gen00.lisp
;; (defstruct0, string-hatch for tricky statements) and
;; plan/20260919_01_merge/gen-cpp-freestanding-example.lisp (write-source).
;; Backup rule: cp cpp/gen.lisp cpp/gen.lisp.known-good before editing.
(eval-when (:compile-toplevel :execute :load-toplevel)
  (ql:quickload "cl-cpp-generator2"))
(defpackage #:treemap-cpp-gen
  (:use #:cl #:cl-cpp-generator2))
(in-package #:treemap-cpp-gen)
(defparameter *gen-dir*
  (merge-pathnames #P"gen/" (merge-pathnames #P"./" *load-pathname*)))
;; lprint: one std::cerr line, each var once as [label]=value.
;; A var is an expression or (:as "label" expr).
(defun lprint (&key (msg "") (vars nil))
  (let ((entries (mapcar (lambda (v)
                           (if (logand (consp v) (eq (car v) :as))
                               (list (second v) (third v))
                               (list (emit-c :code v) v)))
                         vars)))
    `(<< "std::cerr"
         (string ,(format nil "~a~{ [~a]='{}'~}" msg (mapcar #'first entries)))
         ,@(mapcar #'second entries)
         "std::endl")))
;; Types block shared by all headers (Rect/Color/Node).
(defun emit-types ()
  `(do0
    (defstruct0 Rect
      (x float)
      (y float)
      (w float)
      (h float))
    (defstruct0 Color
      (r float)
      (g float)
      (b float)
      (a float))
    (defstruct0 Node
      (path "std::filesystem::path")
      (size uintmax_t)
      (isDir bool)
      (children "std::vector<Node>")
      (rect Rect)
      (color Color))))
;; Serial scan emitters (<= 60 lines each).
(defun emit-scan-decls ()
  `(do0
    "inline std::mutex& GetStderrMutex () { static std::mutex m; return m; }"
    (defun report_skip (context path ec)
      (declare (type "const std::string&" context)
               (type "const fs::path&" path)
               (type "const std::error_code&" ec)
               (values void))
      "std::lock_guard lock (GetStderrMutex ());"
      (<< "std::cerr"
          (string "skip [")
          context
          (string "]: ")
          "path.string ()"
          (string ": ")
          (dot ec (message))
          "std::endl"))
    (defun IsVirtualRoot (p)
      (declare (type "const fs::path&" p)
               (values bool))
      (let ((s "p.string ()"))
        (declare (type "const std::string" s))
        (return (logior (dot s (starts_with (string "/proc")))
                    (dot s (starts_with (string "/sys")))
                    (dot s (starts_with (string "/dev")))))))
    "inline Color color_for_path (const fs::path&);"))
(defun emit-scan-serial ()
  `(do0
    (defun scan_entry (node entry)
      (declare (type "Node&" node)
               (type "const fs::directory_entry&" entry)
               (values void))
      "std::error_code ec;"
      (let ((st (dot entry (status ec))))
        (when ec
          (progn
            (report_skip (string "file_type") (dot entry (path)) ec)
            (return)))
        (when (fs--is_symlink (dot entry (symlink_status ec)))
          (return))
        (let ((p (dot entry (path))))
          (if (== (dot st (type)) fs--file_type--directory)
              (let ((child (scan_tree p)))
                (when (< 0 (dot child size))
                  (incf  (dot node size) (dot child size))
                  (dot node children (push_back (std--move child)))))
              (when (== (dot st (type)) fs--file_type--regular)
                (let ((size (dot entry (file_size ec))))
                  (when ec
                    (progn
                      (report_skip (string "metadata") p ec)
                      (return)))
                  (when (logand (< 0 size) (< size (<< "uintmax_t{1}" 48)))
                    (let ((child (Node)))
                      
                      (= (dot child path) p)
                      (= (dot child size) size)
                      (= (dot child isDir) false)
                      (= (dot child color) (color_for_path p))
                      (incf  (dot node size) size)
                      (dot node children (push_back (std--move child)))))))))))
    (defun scan_tree (path)
      (declare (type "const fs::path&" path)
               (values Node))
      (let ((node (Node)))
        
        (= (dot node path) path)
        (when (IsVirtualRoot path)
          (return node))
        "std::error_code ec;"
        "fs::directory_iterator it (path, ec);"
        (when ec
          (progn
            (report_skip (string "read_dir") path ec)
            (return node)))
        "for (; it != fs::directory_iterator (); it.increment (ec)) {"
        "if (ec) { report_skip (\"entry\", path, ec); break; }"
        (scan_entry node (deref it))
        "}"
        (return node)))))
;; Parallel scan: files inline, one thread per subdirectory.
(defun emit-scan-parallel ()
  `(do0
    (defun scan_tree_parallel (path)
      (declare (type "const fs::path&" path)
               (values Node))
      (let ((node Node)
            (subdirs "std::vector<fs::path>"))
        
        (= (dot node path) path)
        (when (IsVirtualRoot path)
          (return node))
        "std::error_code ec;"
        "fs::directory_iterator it (path, ec);"
        (when ec
          (progn
            (report_skip (string "read_dir") path ec)
            (return node)))
        "for (; it != fs::directory_iterator (); it.increment (ec)) {"
        "if (ec) { report_skip (\"entry\", path, ec); break; }"
        (let ((entry (deref it)))
          "std::error_code ec2;"
          (let ((st (dot entry (status ec2))))
            (when ec2
              (progn
                (report_skip (string "file_type") (dot entry (path)) ec2)
                "continue;"))
            (when (fs--is_symlink (dot entry (symlink_status ec2)))
              "continue;")
            (let ((p (dot entry (path))))
              (if (== (dot st (type)) fs--file_type--directory)
                  (dot subdirs (push_back p))
                  (when (== (dot st (type)) fs--file_type--regular)
                    (let ((size (dot entry (file_size ec2))))
                      (when ec2
                        (progn
                          (report_skip (string "metadata") p ec2)
                          "continue;"))
                      (when (logand (< 0 size) (< size (<< "uintmax_t{1}" 48)))
                        (let ((child (Node)))
                          
                          (= (dot child path) p)
                          (= (dot child size) size)
                          (= (dot child isDir) false)
                          (= (dot child color) (color_for_path p))
                          (incf  (dot node size) size)
                          (dot node children
                               (push_back (std--move child)))))))))))
        "}"
        "std::vector<Node> results (subdirs.size ());"
        "std::vector<std::thread> workers;"
        "workers.reserve (subdirs.size ());"
        "for (size_t i = 0; i < subdirs.size (); ++i) {"
        "workers.emplace_back ([&, i] { results[i] = scan_tree_parallel (subdirs[i]); });"
        "}"
        "for (auto& t : workers) { t.join (); }"
        (foreach (child results)
          (when (< 0 (dot child size))
            (incf  (dot node size) (dot child size))
            (dot node children (push_back (std--move child)))))
        (return node)))))
;; Layout emitters: serial rows + parallel subtree driver.
(defun emit-layout ()
  `(do0
    (defun worst_aspect (areas row sum side)
      (declare (type "const std::vector<double>&" areas)
               (type "const std::vector<size_t>&" row)
               (type double sum)
               (type float side)
               (values double))
      (let ((s2 (* (cast double side) (cast double side)))
            (sum2 (* sum sum))
            (worst 0.0f)))
        
        (foreach (i row)
          (let ((r1 (/ (* s2 (aref areas i)) sum2))
                (r2 (/ sum2 (* s2 (aref areas i)))))
            (= worst (std--max worst (std--max r1 r2)))))
        (return worst))
    (defun layout_row (nodes areas row sum r)
      (declare (type "std::vector<Node>&" nodes)
               (type "const std::vector<double>&" areas)
               (type "const std::vector<size_t>&" row)
               (type double sum)
               (type "Rect&" r)
               (values void))
      (let ((side (std--min (dot r w) (dot r h)))
            (thickness (cast double (/ sum (float side))))
            (isHoriz (< (dot r w) (dot r h)))
            (offset (? isHoriz (dot r x) (dot r y)))))
        
        (foreach (i row)
          (let ((itemLen (coerce (* (/ (aref areas i) sum)
                                    (cast double side))
                                 float)))
            (= (dot (aref nodes i) rect)
               (if isHoriz
                   (curly offset (dot r y) itemLen thickness)
                   (curly (dot r x) offset thickness itemLen)))
            (incf  offset itemLen)))
        (if isHoriz
            (progn (incf  (dot r y) thickness) (decf  (dot r h) thickness))
            (progn (incf  (dot r x) thickness) (decf  (dot r w) thickness))))
    (defun squarify_level (nodes rect)
      (declare (type "std::vector<Node>&" nodes)
               (type Rect rect)
               (values void)
               (mutable nodes rect))
      (let ((total "uintmax_t{0}"))
            (areaMult 0.0f)
            (areas "std::vector<double>")
            (row "std::vector<size_t>")
            (rowSum 0.0f))
        
        (foreach (n nodes)
          (incf  total (dot n size)))
        (when (logior (== total 0) (<= (dot rect w) 0.0f) (<= (dot rect h) 0.0f))
          (return))
        (std--sort (dot nodes (begin)) (dot nodes (end))
                   (lambda (a b)
                     (declare (type "const Node&" a) (type "const Node&" b)
                              (values bool))
                     (return (< (dot b size) (dot a size)))))
        (= areaMult (/ (cast double (* (dot rect w) (dot rect h)))
                       (cast double total)))
        (dot areas (reserve (dot nodes (size))))
        (foreach (n nodes)
          (dot areas (push_back (* (cast double (dot n size)) areaMult))))
        (dotimes (i (dot areas (size)))
          (let ((side (std--min (dot rect w) (dot rect h)))
                (nextRow row))
            
            (dot nextRow (push_back i))
            (if (logior (dot row (empty))
                    (<= (worst_aspect areas nextRow (+ rowSum (aref areas i)) side)
                        (worst_aspect areas row rowSum side)))
                (progn (dot row (push_back i)) (incf  rowSum (aref areas i)))
                (progn
                  (layout_row nodes areas row rowSum rect)
                  (= row (curly i))
                  (= rowSum (aref areas i))))))
        (when (not (dot row (empty)))
          (layout_row nodes areas row rowSum rect)))
    (defun squarify (nodes rect)
      (declare (type "std::vector<Node>&" nodes)
               (type Rect rect)
               (values void))
      (squarify_level nodes rect)
      (foreach (node nodes)
        (when (logand (dot node isDir) (< 4.0f (dot node rect w)) (< 4.0f (dot node rect h)))
          (squarify (dot node children) (dot node rect)))))
    (defun needs_recursion (n)
      (declare (type "const Node&" n)
               (values bool))
      (return (logand (dot n isDir) (not (dot n children (empty)))
                   (< 4.0f (dot n rect w)) (< 4.0f (dot n rect h)))))
    (defun layout_subtree_parallel (node out)
      (declare (type Node node)
               (type "Node&" out)
               (values void)
               (mutable node))
      (when (needs_recursion node)
        (squarify_parallel (dot node children) (dot node rect)))
      (= out (std--move node)))
    (defun squarify_parallel (nodes rect)
      (declare (type "std::vector<Node>&" nodes)
               (type Rect rect)
               (values void))
      (squarify_level nodes rect)
      (let ((taken "std::vector<Node>")
            (dirIdx "std::vector<size_t>"))
        
        (dot taken (swap nodes))
        (dotimes (i (dot taken (size)))
          (when (needs_recursion (aref taken i))
            (dot dirIdx (push_back i))))
        "std::vector<std::thread> workers;"
        "workers.reserve (dirIdx.size ());"
        "for (size_t k = 0; k < dirIdx.size (); ++k) {"
        "workers.emplace_back ([&, k] { Node done; layout_subtree_parallel (std::move (taken[dirIdx[k]]), done); taken[dirIdx[k]] = std::move (done); });"
        "}"
        "for (auto& t : workers) { t.join (); }"
        (std--sort (dot taken (begin)) (dot taken (end))
                   (lambda (a b)
                     (declare (type "const Node&" a) (type "const Node&" b)
                              (values bool))
                     (return (< (dot b size) (dot a size)))))
        (= nodes (std--move taken))))))
;; Color + format_bytes emitters.
(defun emit-color ()
  `(do0
    (defun hash_color (path)
      (declare (type "const fs::path&" path)
               (values Color))
      (let ((name "path.filename ().string ()")
            (h "uint32_t{0}"))
        
        (foreach (b name)
          (incf  h b))
        (return (curly (/ (float (+ (% (* h 37) 160) 80)) 255.0f)
                       (/ (float (+ (% (* h 59) 160) 80)) 255.0f)
                       (/ (float (+ (% (* h 83) 160) 80)) 255.0f)
                       1.0f))))
    (defun color_for_path (path)
      (declare (type "const fs::path&" path)
               (values Color))
      (let ((ext "path.extension ().string ()"))
        (if (logior (== ext (string ".rs")) (== ext (string ".c"))
                (== ext (string ".cpp")) (== ext (string ".py"))
                (== ext (string ".js")) (== ext (string ".ts"))
                (== ext (string ".txt")) (== ext (string ".md")))
            (return (curly 0.2f 0.75f 0.45f 1.0f))
            (if (logior (== ext (string ".png")) (== ext (string ".jpg"))
                    (== ext (string ".jpeg")) (== ext (string ".svg"))
                    (== ext (string ".webp")))
                (return (curly 0.2f 0.65f 0.95f 1.0f))
                (if (logior (== ext (string ".mp4")) (== ext (string ".mkv"))
                        (== ext (string ".mov")) (== ext (string ".mp3"))
                        (== ext (string ".flac")))
                    (return (curly 0.75f 0.35f 0.85f 1.0f))
                    (if (logior (== ext (string ".zip")) (== ext (string ".tar"))
                            (== ext (string ".gz")) (== ext (string ".7z")))
                        (return (curly 0.9f 0.3f 0.25f 1.0f))
                        (return (hash_color path))))))))
    (defun format_bytes (b)
      (declare (type uintmax_t b)
               (values "std::string"))
      (let ((units (curly (string "B") (string "KB") (string "MB")
                            (string "GB") (string "TB")))
            (d (float b))
            (i 0))
        
        (while (logand (<= 1024.0 d) (< i 4))
          (/= d 1024.0)
          (incf i))
        "char buf[32];"
        "snprintf (buf, sizeof (buf), \"%.1f %s\", d, units[i]);"
        (return buf)))))
(progn
  (ensure-directories-exist *gen-dir*)
  (write-source "010_scanner.hpp"
                `(do0
                  "#pragma once"
                  (include<> atomic cstdint filesystem iostream mutex string
                             thread vector)
                  "namespace fs = std::filesystem;"
                  (namespace treemap
                               ,(emit-types)
                               ,(emit-scan-decls)
                               ,(emit-scan-serial)
                               ,(emit-scan-parallel)))
                :dir *gen-dir* :format nil :tidy nil :omit-parens t)
  (write-source "015_color.hpp"
                `(do0
                  "#pragma once"
                  (include<> string)
                  (include "010_scanner.hpp")
                  (namespace treemap
                               ,(emit-color)))
                :dir *gen-dir* :format nil :tidy nil :omit-parens t)
  (write-source "020_layout.hpp"
                `(do0
                  "#pragma once"
                  (include<> algorithm cmath limits thread vector)
                  (include "010_scanner.hpp")
                  (namespace treemap
                               ,(emit-layout)))
                :dir *gen-dir* :format nil :tidy nil :omit-parens t))
