;;; ada83.el --- Ada 83 support: syntax, semantic highlighting, LSP  -*- lexical-binding: t; -*-

;; Author: the ada83 compiler
;; Version: 1.0
;; Keywords: languages, ada
;; Package-Requires: ((emacs "26.1"))

;;; Commentary:

;; Ada 83 support for Emacs in one file: put it on your `load-path' and add
;;
;;     (require 'ada83)
;;
;; to your init file, or just `M-x load-file' it.
;;
;; What it gives you:
;;
;;   * `ada83-mode' for .ada, .ads, .adb and .a files, with a font-lock
;;     that needs nothing but Emacs -- the reserved words, the literals,
;;     the attributes, the pragmas, the labels and the declared names.
;;
;;   * Semantic highlighting from the compiler itself.  `ada83 --highlight'
;;     prints every token of a file with the kind the analysis resolved it
;;     to, so a name is coloured as the package, type, function, variable,
;;     parameter or enumeration literal it was declared as, rather than as
;;     whatever a regular expression guessed.  It is laid over font-lock as
;;     overlays when the file is opened and after it is saved; turn it off
;;     with `ada83-semantic-highlight', or ask for it with
;;     `M-x ada83-highlight-buffer'.
;;
;;   * The language server, `ada83 --lsp', registered with both Eglot and
;;     lsp-mode, so `M-x eglot' or `M-x lsp' in an Ada 83 buffer brings up
;;     diagnostics, completion, hover, definitions and the rest.  Set
;;     `ada83-lsp-autostart' to have Eglot started for you.

;;; Code:

(require 'json)

(defgroup ada83 nil
  "Ada 83 support driven by the ada83 compiler."
  :group 'languages
  :prefix "ada83-")

(defcustom ada83-command "ada83"
  "The ada83 compiler, looked up on `exec-path' when it is not a full path."
  :type 'string
  :group 'ada83)

(defcustom ada83-semantic-highlight t
  "Whether to colour a buffer from `ada83 --highlight' as well as font-lock."
  :type 'boolean
  :group 'ada83)

(defcustom ada83-highlight-limit 20000
  "How many tokens of one buffer semantic highlighting will colour."
  :type 'integer
  :group 'ada83)

(defcustom ada83-lsp-autostart nil
  "Whether entering `ada83-mode' starts Eglot for the buffer."
  :type 'boolean
  :group 'ada83)

(defconst ada83-reserved-words
  '("abort" "abs" "accept" "access" "all" "and" "array" "at" "begin" "body"
    "case" "constant" "declare" "delay" "delta" "digits" "do" "else" "elsif"
    "end" "entry" "exception" "exit" "for" "function" "generic" "goto" "if"
    "in" "is" "limited" "loop" "mod" "new" "not" "of" "or" "others" "out"
    "package" "pragma" "private" "procedure" "raise" "range" "record" "rem"
    "renames" "return" "reverse" "select" "separate" "subtype" "task"
    "terminate" "then" "type" "use" "when" "while" "with" "xor")
  "The reserved words of ANSI/MIL-STD-1815A, less `null', which is a value.")

(defconst ada83-predefined-types
  '("Integer" "Natural" "Positive" "Float" "Boolean" "Character" "String"
    "Duration" "Short_Integer" "Long_Integer" "Short_Float" "Long_Float"
    "Short_Short_Integer" "Long_Long_Integer" "Universal_Integer"
    "Universal_Real" "File_Type" "File_Mode" "Address" "Priority")
  "The types Standard and the predefined library declare.")

(defconst ada83-predefined-packages
  '("Standard" "ASCII" "System" "Calendar" "Text_IO" "Sequential_IO"
    "Direct_IO" "IO_Exceptions" "Low_Level_IO" "Machine_Code" "Integer_IO"
    "Float_IO" "Fixed_IO" "Enumeration_IO" "Unchecked_Conversion"
    "Unchecked_Deallocation")
  "The units of the predefined environment.")

(defconst ada83-predefined-exceptions
  '("Constraint_Error" "Numeric_Error" "Program_Error" "Storage_Error"
    "Tasking_Error" "Status_Error" "Mode_Error" "Name_Error" "Use_Error"
    "Device_Error" "End_Error" "Data_Error" "Layout_Error" "Time_Error")
  "The exceptions the predefined environment declares.")

;;; Faces

;; Every kind is its own face, so a theme can restyle Ada 83 without
;; touching font-lock itself.  Each inherits the face that fits it best,
;; then the one an older Emacs has instead: an inherited face that does not
;; exist contributes nothing, and the first of a list wins, so Emacs 29 and
;; Emacs 26 both land somewhere sensible.

(defface ada83-comment '((t :inherit font-lock-comment-face))
  "Face for an Ada 83 comment." :group 'ada83)
(defface ada83-string '((t :inherit font-lock-string-face))
  "Face for an Ada 83 string literal." :group 'ada83)
(defface ada83-character '((t :inherit font-lock-string-face))
  "Face for an Ada 83 character literal." :group 'ada83)
(defface ada83-number
  '((t :inherit (font-lock-number-face font-lock-constant-face)))
  "Face for an Ada 83 numeric literal." :group 'ada83)
(defface ada83-keyword '((t :inherit font-lock-keyword-face))
  "Face for an Ada 83 reserved word." :group 'ada83)
(defface ada83-attribute
  '((t :inherit (font-lock-property-use-face font-lock-builtin-face)))
  "Face for an attribute designator, the name after a tick." :group 'ada83)
(defface ada83-pragma '((t :inherit font-lock-preprocessor-face))
  "Face for the identifier of a pragma." :group 'ada83)
(defface ada83-label '((t :inherit font-lock-constant-face))
  "Face for a statement label or a loop or block name." :group 'ada83)
(defface ada83-operator '((t :inherit font-lock-operator-face))
  "Face for an Ada 83 operator." :group 'ada83)
(defface ada83-delimiter '((t :inherit font-lock-delimiter-face))
  "Face for an Ada 83 delimiter." :group 'ada83)
(defface ada83-namespace '((t :inherit font-lock-type-face))
  "Face for the name of a package or a generic unit." :group 'ada83)
(defface ada83-type '((t :inherit font-lock-type-face))
  "Face for the name of a type or a subtype." :group 'ada83)
(defface ada83-function '((t :inherit font-lock-function-name-face))
  "Face for the name of a subprogram or an entry." :group 'ada83)
(defface ada83-variable '((t :inherit font-lock-variable-name-face))
  "Face for the name of an object, a component or an exception."
  :group 'ada83)
(defface ada83-parameter
  '((t :inherit (font-lock-variable-use-face font-lock-variable-name-face)))
  "Face for the name of a formal parameter." :group 'ada83)
(defface ada83-enumeration '((t :inherit font-lock-constant-face))
  "Face for an enumeration literal." :group 'ada83)

(defconst ada83-kind-faces
  '(("comment"     . ada83-comment)
    ("string"      . ada83-string)
    ("character"   . ada83-character)
    ("number"      . ada83-number)
    ("keyword"     . ada83-keyword)
    ("attribute"   . ada83-attribute)
    ("pragma"      . ada83-pragma)
    ("label"       . ada83-label)
    ("operator"    . ada83-operator)
    ("delimiter"   . ada83-delimiter)
    ("namespace"   . ada83-namespace)
    ("type"        . ada83-type)
    ("function"    . ada83-function)
    ("variable"    . ada83-variable)
    ("parameter"   . ada83-parameter)
    ("enumeration" . ada83-enumeration)
    ("identifier"  . nil))
  "The face each kind of `ada83 --highlight' is drawn in.
A kind mapped to nil is left to font-lock: an unresolved name should keep
whatever the syntax made of it.")

;;; Syntax and font-lock

(defvar ada83-mode-syntax-table
  (let ((table (make-syntax-table)))
    ;; A comment opens on two hyphens and closes at the end of the line, so
    ;; the hyphen is both a punctuation character and a comment starter.
    (modify-syntax-entry ?-  ". 12" table)
    (modify-syntax-entry ?\n ">"    table)
    (modify-syntax-entry ?_  "_"    table)
    ;; A tick is punctuation, never a string delimiter: were it a delimiter,
    ;; the tick of T'First would open a string that never closes.
    (modify-syntax-entry ?\' "."    table)
    (modify-syntax-entry ?\" "\""   table)
    (modify-syntax-entry ?&  "."    table)
    (modify-syntax-entry ?+  "."    table)
    (modify-syntax-entry ?*  "."    table)
    (modify-syntax-entry ?/  "."    table)
    (modify-syntax-entry ?<  "."    table)
    (modify-syntax-entry ?>  "."    table)
    (modify-syntax-entry ?=  "."    table)
    (modify-syntax-entry ?|  "."    table)
    (modify-syntax-entry ?\\ "."    table)
    table)
  "Syntax table for `ada83-mode'.")

(defconst ada83-font-lock-keywords
  (let ((name "[A-Za-z][A-Za-z0-9_]*"))
    `(;; A character literal only where no value stands before the tick;
      ;; after a name or a ')' the tick marks an attribute instead.
      ("\\(?:\\_>\\|)\\)[ \t]*'\\([A-Za-z][A-Za-z0-9_]*\\)"
       1 'ada83-attribute)
      ("\\(?:^\\|[^A-Za-z0-9_)]\\)\\('.'\\)" 1 'ada83-character)
      (,(concat "\\_<\\(?:pragma\\)[ \t]+\\(" name "\\)")
       1 'ada83-pragma)
      (,(concat "\\_<\\(?:procedure\\|function\\|entry\\|accept\\)[ \t]+\\("
                name "\\|\"[^\"]+\"\\)")
       1 'ada83-function)
      (,(concat "\\_<\\(?:type\\|subtype\\)[ \t]+\\(" name "\\)")
       1 'ada83-type)
      (,(concat "\\_<task[ \t]+\\(?:body[ \t]+\\|type[ \t]+\\)?\\(" name "\\)")
       1 'ada83-type)
      (,(concat "\\_<package[ \t]+\\(?:body[ \t]+\\)?\\("
                name "\\(?:\\." name "\\)*\\)")
       1 'ada83-namespace)
      (,(concat "<<[ \t]*\\(" name "\\)[ \t]*>>")
       1 'ada83-label)
      ("<<\\|>>" . 'ada83-delimiter)
      (,(concat "^[ \t]*\\(" name "\\)[ \t]*:[ \t]*\\(?:for\\_>\\|"
                "while\\_>\\|loop\\_>\\|declare\\_>\\|begin\\_>\\|$\\)")
       1 'ada83-label)
      ;; A name before a colon that is not ":=" is being declared -- an
      ;; object, a component or a formal parameter -- and the name between
      ;; `for' and `in' is a loop parameter.
      (,(concat "\\(" name "\\)[ \t]*:\\(?:[^=]\\|$\\)")
       1 'ada83-variable)
      (,(concat "\\_<for[ \t]+\\(" name "\\)[ \t]+in\\_>")
       1 'ada83-variable)
      (,(concat "\\_<\\(?:true\\|false\\|null\\)\\_>")
       . 'ada83-enumeration)
      (,(regexp-opt ada83-predefined-exceptions 'symbols)
       . 'ada83-type)
      (,(regexp-opt ada83-predefined-types 'symbols)
       . 'ada83-type)
      (,(regexp-opt ada83-predefined-packages 'symbols)
       . 'ada83-namespace)
      (,(regexp-opt ada83-reserved-words 'symbols)
       . 'ada83-keyword)
      ("\\_<[0-9][0-9_]*#[0-9A-Fa-f_]+\\(?:\\.[0-9A-Fa-f_]+\\)?#\\(?:[Ee][-+]?[0-9_]+\\)?"
       . 'ada83-number)
      ("\\_<[0-9][0-9_]*\\(?:\\.[0-9_]+\\)?\\(?:[Ee][-+]?[0-9_]+\\)?"
       . 'ada83-number)
      (":=\\|=>\\|\\.\\.\\|\\*\\*\\|/=\\|>=\\|<=\\|<>\\|[-+*/&<>=|]"
       . 'ada83-operator)))
  "Font-lock rules for `ada83-mode', matched case-insensitively.")

;;; Semantic highlighting

(defun ada83--syntactic-face (state)
  "Return the ada83 face for the comment or string STATE is inside.
A comment and a string are found by the syntax table rather than by a
rule, so this is what routes them to the same faces as everything else."
  (cond ((nth 3 state) 'ada83-string)
        ((nth 4 state) 'ada83-comment)))

(defvar-local ada83--overlays nil
  "The overlays `ada83-highlight-buffer' laid over this buffer.")

(defun ada83--executable ()
  "Return the ada83 compiler if it can be run, else nil."
  (or (and (file-name-absolute-p ada83-command)
           (file-executable-p ada83-command)
           ada83-command)
      (executable-find ada83-command)))

(defun ada83--root-directory (path)
  "Return the directory the compiler should search for the units PATH withs.
That is the nearest ancestor holding a project file, else PATH's own
directory."
  (let ((directory (file-name-directory (expand-file-name path))))
    (or (locate-dominating-file
         directory
         (lambda (candidate)
           (or (directory-files candidate nil "\\.gpr\\'" t)
               (directory-files candidate nil "\\.gpj\\'" t))))
        directory)))

(defun ada83--parse-json (text)
  "Parse TEXT as JSON into alists, or return nil when it is not JSON."
  (condition-case nil
      (if (fboundp 'json-parse-string)
          (json-parse-string text :object-type 'alist :array-type 'list)
        (let ((json-object-type 'alist) (json-array-type 'list))
          (json-read-from-string text)))
    (error nil)))

(defun ada83-highlight-clear ()
  "Remove the semantic highlighting from this buffer."
  (interactive)
  (mapc #'delete-overlay ada83--overlays)
  (setq ada83--overlays nil))

(defun ada83--run-highlight (path)
  "Run `ada83 --highlight' over PATH and return what it parsed to, or nil."
  (let ((compiler (ada83--executable)))
    (when compiler
      (with-temp-buffer
        (let ((status (call-process compiler nil (list (current-buffer) nil)
                                    nil "--highlight" path
                                    (ada83--root-directory path))))
          (and (eq status 0)
               (ada83--parse-json (buffer-string))))))))

(defun ada83-highlight-buffer (&optional spoken)
  "Colour this buffer from the kinds `ada83 --highlight' resolved.
Interactively, or with SPOKEN non-nil, say why nothing happened."
  (interactive (list t))
  (cond
   ((not (derived-mode-p 'ada83-mode))
    (when spoken (message "ada83: not an Ada 83 buffer")))
   ((not (ada83--executable))
    (when spoken (message "ada83: %s is not on the path" ada83-command)))
   ;; The compiler reads the file from disk, so an unsaved buffer would be
   ;; coloured from a stale text; leave font-lock alone until it is saved.
   ((or (not buffer-file-name) (buffer-modified-p))
    (when spoken (message "ada83: save the buffer first")))
   (t
    (let* ((answer (ada83--run-highlight buffer-file-name))
           (tokens (and answer (alist-get 'tokens answer)))
           (legend (append (and answer (alist-get 'legend answer)) nil))
           (applied 0)
           (line 0)
           (position (point-min)))
      (if (not tokens)
          (when spoken (message "ada83: --highlight reported nothing"))
        (ada83-highlight-clear)
        (save-excursion
          (dolist (token tokens)
            (when (< applied ada83-highlight-limit)
              (let* ((kind (nth (nth 3 token) legend))
                     (face (cdr (assoc kind ada83-kind-faces))))
                (when face
                  ;; The tokens arrive in source order, so each line is
                  ;; reached by stepping on from the line before it.
                  (unless (= line (nth 0 token))
                    (setq line (nth 0 token))
                    (goto-char (point-min))
                    (forward-line (1- line))
                    (setq position (position-bytes (point))))
                  (let ((start (byte-to-position
                                (+ position (1- (nth 1 token)))))
                        (end   (byte-to-position
                                (+ position (1- (nth 1 token)) (nth 2 token)))))
                    (when (and start end (< start end))
                      (let ((overlay (make-overlay start end nil t nil)))
                        (overlay-put overlay 'face face)
                        (overlay-put overlay 'ada83 t)
                        (overlay-put overlay 'evaporate t)
                        (push overlay ada83--overlays)
                        (setq applied (1+ applied))))))))))
        (when spoken (message "ada83: coloured %d tokens" applied)))))))

(defun ada83--highlight-after-save ()
  "Recolour the buffer when it is saved, if semantic highlighting is on."
  (when (and ada83-semantic-highlight (derived-mode-p 'ada83-mode))
    (ada83-highlight-buffer)))

;;; The mode

(defvar ada83-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map (kbd "C-c C-h") #'ada83-highlight-buffer)
    (define-key map (kbd "C-c C-l") #'ada83-highlight-clear)
    map)
  "Keymap for `ada83-mode'.")

;;;###autoload
(define-derived-mode ada83-mode prog-mode "Ada83"
  "Major mode for Ada 83, coloured by the ada83 compiler.

\\{ada83-mode-map}"
  :syntax-table ada83-mode-syntax-table
  (setq-local comment-start "-- ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "--+[ \t]*")
  (setq-local case-fold-search t)
  (setq-local font-lock-defaults
              '(ada83-font-lock-keywords nil t nil nil
                (font-lock-syntactic-face-function
                 . ada83--syntactic-face)))
  (setq-local indent-tabs-mode nil)
  (add-hook 'after-save-hook #'ada83--highlight-after-save nil t)
  (when ada83-semantic-highlight
    (ada83-highlight-buffer))
  (when (and ada83-lsp-autostart (fboundp 'eglot-ensure))
    (eglot-ensure)))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.ad[abs]\\'" . ada83-mode))
;;;###autoload
(add-to-list 'auto-mode-alist '("\\.ada\\'" . ada83-mode))

;;; The language server

(with-eval-after-load 'eglot
  (add-to-list 'eglot-server-programs
               `(ada83-mode . (,ada83-command "--lsp"))))

(with-eval-after-load 'lsp-mode
  (when (fboundp 'lsp-register-client)
    (add-to-list 'lsp-language-id-configuration '(ada83-mode . "ada83"))
    (lsp-register-client
     (make-lsp-client
      :new-connection (lsp-stdio-connection
                       (lambda () (list ada83-command "--lsp")))
      :major-modes '(ada83-mode)
      :server-id 'ada83))))

(provide 'ada83)

;;; ada83.el ends here
