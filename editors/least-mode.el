;;; least-mode.el --- Major Mode for editing Least programs code -*- lexical-binding: t -*-
;;; Commentary:
;;
;; Major Mode for editing Least programs code.
;;; Code:

(defvar least-mode-hook nil)

(eval-and-compile
  (defconst least-keywords
    '("if" "elif" "else" "end" "print" "read" "exit" "int" "str" "set" "add" "putchar" "while")))

(defconst least-highlights
  `((,(regexp-opt least-keywords 'symbols) . font-lock-keyword-face)))

;;;###autoload
(define-derived-mode least-mode prog-mode "least"
  "Major Mode for editing Least source code."
  (setq font-lock-defaults '(least-highlights)))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.least\\'" . least-mode))

(provide 'least-mode)

;;; least-mode.el ends here
