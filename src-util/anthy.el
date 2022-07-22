;;; anthy.el -- Anthy

;; Copyright (C) 2001 - 2007 KMC(Kyoto University Micro Computer Club)

;; Author: Yusuke Tabata<yusuke@kmc.gr.jp>
;;         Tomoharu Ugawa
;;         Norio Suzuki <suzuki@sanpobu.net>
;; Keywords: japanese

;; This file is part of Anthy

;;; Commentary:
;;
;; $B$+$J4A;zJQ49%(%s%8%s(B Anthy$B$r(B emacs$B$+$i;H$&$?$a$N%W%m%0%i%`(B
;; Anthy$B%i%$%V%i%j$r;H$&$?$a$N%3%^%s%I(Banthy-agent$B$r5/F0$7$F!"(B
;; anthy-agent$B$H%Q%$%W$GDL?.$r$9$k$3$H$K$h$C$FJQ49$NF0:n$r9T$&(B
;;
;;
;; Funded by IPA$BL$F'%=%U%H%&%'%"AOB$;v6H(B 2001 10/10
;;
;; $B3+H/$O(Bemacs21.2$B>e$G9T$C$F$$$F(Bminor-mode
;; $B$b$7$/$O(Bleim$B$H$7$F$b;HMQ$G$-$k(B
;; (set-input-method 'japanese-anthy)
;;
;; emacs19(mule),20,21,xemacs$B$GF0:n$9$k(B
;;
;;
;; 2003-08-24 XEmacs $B$N8uJdA*Br%b!<%I%P%0$KBP1~(B(suzuki)
;;
;; 2001-11-16 EUC-JP -> ISO-2022-JP
;;
;; TODO
;;  $B8uJdA*Br%b!<%I$G8uJd$r$$$C$-$K<!$N%Z!<%8$K$$$+$J$$$h$&$K$9$k(B(2ch$B%9%l(B78)
;;  minibufffer$B$N07$$(B
;;  isearch$BBP1~(B
;;
;; $BMQ8l(B
;;  commit $BJ8;zNs$r3NDj$9$k$3$H(B
;;  preedit($B%W%j%(%G%#%C%H(B) $B3NDjA0$NJ8;zNs%"%s%@!<%i%$%s$d6/D4$NB0@-$b4^$`(B
;;  segment($BJ8@a(B) $BJ8K!E*$JJ8@a$G$O$J$/!$F1$8B0@-$NJ8;zNs$N$+$?$^$j(B
;;

;;; Code:
;(setq debug-on-error t)

(defvar anthy-default-enable-enum-candidate-p t
  "$B$3$l$r@_Dj$9$k$H<!8uJd$r?t2s2!$7$?:]$K8uJd$N0lMw$+$iA*Br$9$k%b!<%I$K$J$j$^$9!%(B")

(defvar anthy-personality ""
  "$B%Q!<%=%J%j%F%#(B")

(defvar anthy-preedit-begin-mark "|"
  "$BJQ49;~$N@hF,$KIU$/J8;zNs(B")

(defvar anthy-preedit-delim-mark "|"
 "$BJQ49;~$NJ8@a$N6h@Z$j$K;H$o$l$kJ8;zNs(B")

(defvar anthy-accept-timeout 50)
(if (or
     (string-match "^22\." emacs-version)
     (string-match "^23\." emacs-version))
    (setq anthy-accept-timeout 1))

(defconst anthy-working-buffer " *anthy*")
(defvar anthy-agent-process nil
  "anthy-agent$B$N%W%m%;%9(B")
(defvar anthy-use-hankaku-kana t)
;;
(defvar anthy-agent-command-list '("anthy-agent")
  "anthy-agent$B$N(BPATH$BL>(B")

;; face
(defvar anthy-hilight-face nil)
(defvar anthy-underline-face nil)
(copy-face 'highlight 'anthy-highlight-face)
(set-face-underline-p 'anthy-highlight-face t)
(copy-face 'underline 'anthy-underline-face)

;;
(defvar anthy-xemacs
  (if (featurep 'xemacs)
      t nil))
(if anthy-xemacs
    (require 'overlay))
;;
(defvar anthy-mode-map nil
  "Anthy$B$N(BASCII$B%b!<%I$N%-!<%^%C%W(B")
(or anthy-mode-map
    (let ((map (make-keymap))
	  (i 32))
      (define-key map (char-to-string 10) 'anthy-insert)
      (define-key map (char-to-string 17) 'anthy-insert)
      (while (< i 127)
	(define-key map (char-to-string i) 'anthy-insert)
	(setq i (+ 1 i)))
      (setq anthy-mode-map map)))
;;
(defvar anthy-preedit-keymap nil
  "Anthy$B$N(Bpreedit$B$N%-!<%^%C%W(B")
(or anthy-preedit-keymap
    (let ((map (make-keymap))
	  (i 0))
      ;; $BDL>o$NJ8;z$KBP$7$F(B
      (while (< i 128)
	(define-key map (char-to-string i) 'anthy-insert)
	(setq i (+ 1 i)))
      ;; $BJ8@a$N?-=L(B
      (define-key map [(shift left)] 'anthy-insert)
      (define-key map [(shift right)] 'anthy-insert)
      ;; $BJ8@a$N0\F0(B
      (define-key map [left] 'anthy-insert)
      (define-key map [right] 'anthy-insert)
      (define-key map [backspace] 'anthy-insert)
      (setq anthy-preedit-keymap map)))

;; anthy-agent$B$KAw$k:]$K%-!<$r%(%s%3!<%I$9$k$?$a$N%F!<%V%k(B
(defvar anthy-keyencode-alist
  '((1 . "(ctrl A)") ;; \C-a
    (2 . "(left)") ;; \C-b
    (4 . "(ctrl D)") ;; \C-d
    (5 . "(ctrl E)") ;; \C-e
    (6 . "(right)") ;; \C-f
    (7 . "(esc)") ;; \C-g
    (8 . "(ctrl H)") ;; \C-h
    (9 . "(shift left)") ;; \C-i
    (10 . "(ctrl J)")
    (11 . "(ctrl K)")
    (13 . "(enter)") ;; \C-m
    (14 . "(space)") ;; \C-n
    (15 . "(shift right)") ;; \C-o
    (16 . "(up)") ;; \C-p
    (32 . "(space)")
    (40 . "(opar)") ;; '('
    (41 . "(cpar)") ;; ')'
    (127 . "(ctrl H)")
    ;; emacs map
    (S-right . "(shift right)")
    (S-left . "(shift left)")
    (right . "(right)")
    (left . "(left)")
    (up . "(up)")
    (backspace . "(ctrl H)")
    ;; xemacs
    ((shift right) . "(shift right)")
    ((shift left) . "(shift left)")
    ((right) . "(right)")
    ((left) . "(left)")
    ((up) . "(up)"))
  "$B%-!<$N%$%Y%s%H$r(Banthy-agent$B$KAw$k$?$a$NBP1~I=(B")

;; $B%b!<%I%i%$%s$NJ8;zNs(B
(defvar anthy-mode-line-string-alist
  '(("hiragana" . " $B$"(B")
    ("katakana" . " $B%"(B")
    ("alphabet" . " A")
    ("walphabet" . " $B#A(B")
    ("hankaku_kana" . " (I1(B")
    )
  "$B%b!<%IL>$H%b!<%I%i%$%s$NJ8;zNs$NBP1~I=(B")

;; $B:G8e$K3d$jEv$F$?(Bcontext id
(defvar anthy-last-context-id 1)

;; From skk-macs.el From viper-util.el.  Welcome!
(defmacro anthy-deflocalvar (var default-value &optional documentation)
  `(progn
       (defvar ,var ,default-value
	 ,(format "%s\n\(buffer local\)" documentation))
       (make-variable-buffer-local ',var)
       ))

;; buffer local variables
(anthy-deflocalvar anthy-context-id nil "$B%3%s%F%-%9%H$N(Bid")
; $B%b!<%I$N4IM}(B
(anthy-deflocalvar anthy-minor-mode nil)
(anthy-deflocalvar anthy-mode nil)
(anthy-deflocalvar anthy-leim-active-p nil)
(anthy-deflocalvar anthy-saved-mode nil)
; $B%W%j%(%G%#%C%H(B
(anthy-deflocalvar anthy-preedit "")
(anthy-deflocalvar anthy-preedit-start 0)
(anthy-deflocalvar anthy-preedit-overlays '())
(anthy-deflocalvar anthy-mode-line-string " A")
; $B8uJdNs5s(B
(anthy-deflocalvar anthy-enum-candidate-p nil)
(anthy-deflocalvar anthy-enum-rcandidate-p nil)
(anthy-deflocalvar anthy-candidate-minibuffer "")
(anthy-deflocalvar anthy-enum-candidate-list '()
		   "$B:#Ns5s$7$F$$$k8uJd$N>pJs(B(($B2hLLFb$N(Bindex $B8uJd$N(Bindex . $B8uJdJ8;zNs(B) ..)")
(anthy-deflocalvar anthy-enable-enum-candidate-p 
  (cons anthy-default-enable-enum-candidate-p nil)
  "$B$3$N%P%C%U%!$G8uJd$NNs5s$r9T$&$+$I$&$+(B")
(anthy-deflocalvar anthy-current-candidate-index 0)
(anthy-deflocalvar anthy-current-candidate-layout-begin-index 0)
(anthy-deflocalvar anthy-current-candidate-layout-end-index 0)
; $BF~NO>uBV(B
(anthy-deflocalvar anthy-current-rkmap "hiragana")
; undo
(anthy-deflocalvar anthy-buffer-undo-list-saved nil)

;;
(defvar anthy-wide-space "$B!!(B" "$B%9%Z!<%9$r2!$7$?;~$K=P$FMh$kJ8;z(B")

;;; setup minor-mode
;; minor-mode-alist
(if (not
     (assq 'anthy-minor-mode minor-mode-alist))
    (setq minor-mode-alist
       (cons
	(cons 'anthy-minor-mode '(anthy-mode-line-string))
	minor-mode-alist)))
;; minor-mode-map-alist
(if (not
     (assq 'anthy-minor-mode minor-mode-map-alist))
    (setq minor-mode-map-alist
       (cons
	(cons 'anthy-minor-mode anthy-mode-map)
	minor-mode-map-alist)))

;;
(defun anthy-process-sentinel (proc stat)
  "$B%W%m%;%9$N>uBV$,JQ2=$7$?$i;2>H$r>C$7$F!$<!$K:F5/F0$G$-$k$h$&$K$9$k(B"
  (message "%s" stat)
  (anthy-mode-off)
  (setq anthy-agent-process nil))

;;; status
(defun anthy-update-mode-line ()
  "$B%b!<%I%i%$%s$r99?7$9$k(B"
  (let ((a (assoc anthy-current-rkmap anthy-mode-line-string-alist)))
    (if a
	(progn
	 (setq anthy-mode-line-string (cdr a))
	 (setq current-input-method-title
	       (concat "<Anthy:" (cdr a) ">")))))
  (force-mode-line-update))

;;; preedit control
(defun anthy-erase-preedit ()
  "$B%W%j%(%G%#%C%H$rA4It>C$9(B"
  (if (> (string-width anthy-preedit) 0)
      (let* ((str anthy-preedit)
	     (len (length str))
	     (start anthy-preedit-start))
	(delete-region start (+ start len))
	(goto-char start)))
  (setq anthy-preedit "")
  (mapcar 'delete-overlay anthy-preedit-overlays)
  (setq anthy-preedit-overlays nil))

(defun anthy-select-face-by-attr (attr)
  "$BJ8@a$NB0@-$K1~$8$?(Bface$B$rJV$9(B"
  (if (memq 'RV attr)
      'anthy-highlight-face
    'anthy-underline-face))

(defun anthy-enable-preedit-keymap ()
  "$B%-!<%^%C%W$r%W%j%(%G%#%C%H$NB8:_$9$k;~$N$b$N$K@ZBX$($k(B"
;  (setq anthy-saved-buffer-undo-list buffer-undo-list)
;  (buffer-disable-undo)
  (setcdr
   (assq 'anthy-minor-mode minor-mode-map-alist)
   anthy-preedit-keymap))

(defun anthy-disable-preedit-keymap ()
  "$B%-!<%^%C%W$r%W%j%(%G%#%C%H$NB8:_$7$J$$;~$N$b$N$K@ZBX$($k(B"
;  (buffer-enable-undo)
;  (setq buffer-undo-list anthy-saved-buffer-undo-list)
  (setcdr
   (assq 'anthy-minor-mode minor-mode-map-alist)
   anthy-mode-map)
  (anthy-update-mode-line))

(defun anthy-insert-preedit-segment (str attr)
  "$B%W%j%(%G%#%C%H$r0lJ8@aJ8DI2C$9$k(B"
  (let ((start (point))
	(end) (ol))
    (cond ((or (memq 'ENUM attr) (memq 'ENUMR attr))
	   (setq str (concat "<" str ">")))
	  ((memq 'RV attr) 
	   (setq str (concat "[" str "]"))))
    ; $B%W%j%(%G%#%C%H$NJ8;zNs$rDI2C$9$k(B
    (insert-and-inherit str)
    (setq end (point))
    ;; overlay$B$K$h$C$FB0@-$r@_Dj$9$k(B
    (setq ol (make-overlay start end))
    (overlay-put ol 'face (anthy-select-face-by-attr attr))
    (setq anthy-preedit-overlays
	  (cons ol anthy-preedit-overlays))
    str))

(defvar anthy-select-candidate-keybind
  '((0 . "a")
    (1 . "s")
    (2 . "d")
    (3 . "f")
    (4 . "g")
    (5 . "h")
    (6 . "j")
    (7 . "k")
    (8 . "l")
    (9 . ";")))

;;;
;;; auto fill controll
;;; from egg.el

(defun anthy-do-auto-fill ()
  (if (and auto-fill-function (> (current-column) fill-column))
      (let ((ocolumn (current-column)))
	(funcall auto-fill-function)
	(while (and (< fill-column (current-column))
		    (< (current-column) ocolumn))
	  (setq ocolumn (current-column))
	  (funcall auto-fill-function)))))

;;
(defun anthy-check-context-id ()
  "$B%P%C%U%!$K%3%s%F%-%9%H(Bid$B$,3d$j?6$i$l$F$$$k$+$r%A%'%C%/$9$k(B"
  (if (null anthy-context-id)
      (progn
	(setq anthy-context-id anthy-last-context-id)
	(setq anthy-last-context-id
	      (+ anthy-last-context-id 1)))))

(defun anthy-get-candidate (idx)
  "agent$B$+$i8uJd$r0l$D<hF@$9$k(B"
  (anthy-send-recv-command 
   (concat " GET_CANDIDATE "
	   (number-to-string idx) "\n")))

;; $B8uJd%j%9%H$+$i%_%K%P%C%U%!$KI=<($9$kJ8;zNs$r9=@.$9$k(B
(defun anthy-make-candidate-minibuffer-string ()
  (let ((cand-list anthy-enum-candidate-list)
	(cur-elm)
	(str))
    (while cand-list
      (setq cur-elm (car cand-list))
      (let ((cand-str (cdr (cdr cur-elm)))
	    (cand-idx (car (cdr cur-elm)))
	    (sel-idx (car cur-elm)))
	(setq str (format (if (= anthy-current-candidate-index cand-idx)
			      "%s:[%s] "
			      "%s: %s  ")
			  (cdr (assoc sel-idx anthy-select-candidate-keybind))
			  cand-str)))
      (setq anthy-candidate-minibuffer
	    (concat str
		    anthy-candidate-minibuffer))
      (setq cand-list (cdr cand-list)))))

;; $BI=<($9$k8uJd$N%j%9%H$K2hLLFb$G$N%$%s%G%C%/%9$rIU$1$k(B
(defun anthy-add-candidate-index (lst)
  (let ((i 0)
	(res nil))
    (while lst
      (setq res
	    (cons
	     (cons i (car lst))
	     res))
      (setq i (1+ i))
      (setq lst (cdr lst)))
    res))


;; $BJ8;z$NI}$r7W;;$7$F!"I=<($9$k8uJd$N%j%9%H$r:n$k(B
(defun anthy-make-candidate-index-list (base nr l2r)
  (let ((width (frame-width))
	(errorp nil)
	(i 0)
	(repl)
	(cand-idx)
	(lst))
    ;; loop
    (while (and
	    (if l2r
		(< (+ base i) nr)
	      (<= 0 (- base i)))
	    (> width 0)
	    (< i (length anthy-select-candidate-keybind))
	    (not errorp))
      (if l2r
	  (setq cand-idx (+ base i))
	(setq cand-idx (- base i)))
      (setq repl (anthy-get-candidate cand-idx))
      (if (listp repl)
	  ;; append candidate
	  (let ((cand-str (car repl)))
	    (setq width (- width (string-width cand-str) 5))
	    (if (or (> width 0) (null lst))
		(setq lst
		      (cons
		       (cons cand-idx cand-str)
		       lst))))
	;; erroneous candidate
	(setq errorp t))
      (setq i (1+ i)))
    (if errorp
	nil
      lst)))
  

;; $BI=<($9$k8uJd$N%j%9%H$r:n$k(B
(defun anthy-calc-candidate-layout (base nr l2r)
  (let
      ((lst (anthy-make-candidate-index-list base nr l2r)))
    ;;$B%+%l%s%H$N8uJdHV9f$r@_Dj$9$k(B
    (if l2r
	(progn
	  ;; $B:8$+$i1&$N>l9g(B
	  ;; index$B$r0lHV1&$N8uJd$K@_Dj$9$k(B
	  (anthy-get-candidate (car (car lst)))
	  (setq lst (reverse lst))
	  (setq anthy-current-candidate-index (car (car lst))))
      (progn
	;; $B1&$+$i:8$N>l9g(B
	(setq anthy-current-candidate-index (car (car (reverse lst))))))
    ;;$B7k2L$r%;%C%H(B
    (setq anthy-enum-candidate-list
	  (if lst
	      (anthy-add-candidate-index lst)
	    nil))))

;;
(defun anthy-layout-candidate (idx nr)
  "$B8uJd%j%9%H$r(Bminibuffer$B$X%l%$%"%&%H$9$k(B"
  (setq anthy-candidate-minibuffer "")
  (setq anthy-enum-candidate-list '())
  ;; $B:8(B->$B1&(B or $B1&(B->$B:8$K%l%$%"%&%H$9$k(B
  (if anthy-enum-candidate-p
      (anthy-calc-candidate-layout idx nr 't)
    (anthy-calc-candidate-layout idx nr nil))
  (anthy-make-candidate-minibuffer-string)
  ;; $B7k2L$rI=<($9$k(B
  (if anthy-enum-candidate-list
      (progn
	(message "%s" anthy-candidate-minibuffer)
	(setq anthy-current-candidate-layout-begin-index
	      (car (cdr (car (reverse anthy-enum-candidate-list)))))
	(setq anthy-current-candidate-layout-end-index
	      (car (cdr (car anthy-enum-candidate-list)))))

    nil))

(defun anthy-update-preedit (stat ps)
  "$B%W%j%(%G%#%C%H$r99?7$9$k(B"
  (let ((cursor-pos nil)
	(num-candidate 0)
	(idx-candidate 0)
	(enum-candidate
	 (or anthy-enum-candidate-p
	     anthy-enum-rcandidate-p)))
    ;; erase old preedit
    (anthy-erase-preedit)

    ;; $BF~NO%-%c%s%;%k;~$K(Bundo$B%j%9%H$r7R$2$k(B
    (if (and (= (length ps) 0)  anthy-buffer-undo-list-saved )
	(progn
;	  (message "enable")
	  (buffer-enable-undo)
	  (setq buffer-undo-list anthy-buffer-undo-list)
	  (setq anthy-buffer-undo-list-saved nil)
	  ))

    (anthy-disable-preedit-keymap)
    ;; insert new preedit
    (setq anthy-preedit-start (point))
    (setq anthy-enum-candidate-p nil)
    (setq anthy-enum-rcandidate-p nil)
    (if (member stat '(2 3 4))
	(progn
	  (setq anthy-preedit
		(concat anthy-preedit anthy-preedit-begin-mark))
	  (anthy-insert-preedit-segment anthy-preedit-begin-mark '())

	  ;; $BF~NO3+;O$HF1;~$K(Bundo$B%j%9%H$rL58z2=(B
	  (if (not anthy-buffer-undo-list-saved)
	      (progn
		;(message "disable")
		(setq anthy-buffer-undo-list (cdr buffer-undo-list))
		(buffer-disable-undo)
		(setq anthy-buffer-undo-list-saved 't)
		)
	    ;(message "not saved")
	    )

	  ))

    ;; $B3FJ8@a$KBP$7$F(B
    (while ps
      (let ((cur (car ps)))
	(setq ps (cdr ps))
	(cond
	 ((eq cur 'cursor)
	  (setq cursor-pos (point)))
	 ((string-equal (car (cdr cur)) "")
	  nil)
	 (t
	  (let ((nr (car (cdr (cdr (cdr cur)))))
		(idx (car (cdr (cdr cur))))
		(str (car (cdr cur)))
		(attr (car cur)))
	    (setq str (anthy-insert-preedit-segment str attr))
	    (cond ((and (car anthy-enable-enum-candidate-p) (memq 'ENUM attr))
		   ;; $B=gJ}8~$N8uJdNs5s(B
		   (setq anthy-enum-candidate-p t)
		   (setq idx-candidate idx)
		   (setq num-candidate nr))
		  ((and (car anthy-enable-enum-candidate-p) (memq 'ENUMR attr))
		   ;; $B5UJ}8~$N8uJdNs5s(B
		   (setq anthy-enum-rcandidate-p t)
		   (setq idx-candidate idx)
		   (setq num-candidate nr)))
	    (setq anthy-preedit
		  (concat anthy-preedit str))
	    (if (and (member stat '(3 4)) (not (eq ps '())))
		(progn
		  (setq anthy-preedit
			(concat anthy-preedit anthy-preedit-delim-mark))
		  (anthy-insert-preedit-segment anthy-preedit-delim-mark '()))))))))
    ;; $B8uJd0lMw$NI=<(3+;O%A%'%C%/(B
    (if (and (not enum-candidate)
	     (or anthy-enum-candidate-p anthy-enum-rcandidate-p))
	(setq anthy-current-candidate-layout-begin-index 0))
    ;; $B8uJd$NNs5s$r9T$&(B
    (if (or anthy-enum-candidate-p anthy-enum-rcandidate-p)
	(anthy-layout-candidate idx-candidate num-candidate))
    ;; preedit$B$N(Bkeymap$B$r99?7$9$k(B
    (if (member stat '(2 3 4))
	(anthy-enable-preedit-keymap))
    (if cursor-pos (goto-char cursor-pos))))

; suzuki : Emacs / XEmacs $B$G6&DL$N4X?tDj5A(B
(defun anthy-encode-key (ch)
  (let ((c (assoc ch anthy-keyencode-alist)))
    (if c
	(cdr c)
      (if (and
	   (integerp ch)
	   (> ch 32))
	  (char-to-string ch)
	nil))))

(defun anthy-restore-undo-list (commit-str)
  (let* ((len (length commit-str))
	 (beginning (point))
	 (end (+ beginning len)))
    (setq buffer-undo-list
	  (cons (cons beginning end)
		(cons nil anthy-saved-buffer-undo-list)))
	 ))

(defun anthy-proc-agent-reply (repl)
  (let*
      ((stat (car repl))
       (body (cdr repl))
       (commit "")
       (commitlen nil)
       (preedit nil))
    ;; $B3FJ8@a$r=hM}$9$k(B
    (while body
      (let* ((cur (car body))
	     (pe nil))
	(setq body (cdr body))
	(if (and
	     (listp cur)
	     (listp (car cur)))
	    (cond
	     ((eq (car (car cur)) 'COMMIT)
	      (setq commit (concat commit (car (cdr cur)))))
	     ((eq (car (car cur)) 'CUTBUF)
	      (let ((len (length (car (cdr cur)))))
		(copy-region-as-kill (point) (+ (point) len))))
	     ((memq 'UL (car cur))
	      (setq pe (list cur))))
	  (setq pe (list cur)))
	(if pe
	    (setq preedit (append preedit pe)))))
    ;; $B%3%_%C%H$5$l$?J8@a$r=hM}$9$k(B
;    (anthy-restore-undo-list commit)
    (if (> (string-width commit) 0)
	(progn
	  (setq commitlen (length commit))
	  (anthy-erase-preedit)
	  (anthy-disable-preedit-keymap)
	  ; $B@h$K%3%_%C%H$5$;$F$*$/(B
	  (insert-and-inherit commit)
	  (anthy-do-auto-fill)

	  ;; $B%3%_%C%H;~$K7R$2$k(B
	  (if anthy-buffer-undo-list-saved 
	      (progn
		;(message "enable")
		; $BI|5"$5$;$kA0$K!$:#(Bcommit$B$7$?FbMF$r%j%9%H$KDI2C(B
		(setq anthy-buffer-undo-list
		      (cons (cons anthy-preedit-start
				  (+ anthy-preedit-start commitlen))
			    anthy-buffer-undo-list))
		(setq anthy-buffer-undo-list (cons nil anthy-buffer-undo-list))

		(buffer-enable-undo)

		(setq buffer-undo-list anthy-buffer-undo-list)

		(setq anthy-buffer-undo-list-saved nil)
		))

	  (run-hooks 'anthy-commit-hook)
	  ))
    (anthy-update-preedit stat preedit)
    (anthy-update-mode-line)))

(defun anthy-insert-select-candidate (ch)
  (let* ((key-idx (car (rassoc (char-to-string ch)
			       anthy-select-candidate-keybind)))
	 (idx (car (cdr (assq key-idx
			      anthy-enum-candidate-list)))))
    (if idx
	(progn
	  (let ((repl (anthy-send-recv-command
		       (format " SELECT_CANDIDATE %d\n" idx))))
	    (anthy-proc-agent-reply repl))
	  (setq anthy-enum-candidate-p nil)
	  (setq anthy-enum-rcandidate-p nil))
      (message "%s" anthy-candidate-minibuffer))))

(defvar anthy-default-rkmap-keybind
  '(
    ;; q
    (("hiragana" . 113) . "katakana")
    (("katakana" . 113) . "hiragana")
    ;; l
    (("hiragana" . 108) . "alphabet")
    (("katakana" . 108) . "alphabet")
    ;; L
    (("hiragana" . 76) . "walphabet")
    (("katakana" . 76) . "walphabet")
    ;; \C-j
    (("alphabet" . 10) . "hiragana")
    (("walphabet" . 10) . "hiragana")
    ;; \C-q
    (("hiragana" . 17) . "hankaku_kana")
    (("hankaku_kana" . 17) . "hiragana")
    ))


(defvar anthy-rkmap-keybind anthy-default-rkmap-keybind)


(defun anthy-find-rkmap-keybind (ch)
  (let ((res
	 (assoc (cons anthy-current-rkmap ch) anthy-rkmap-keybind)))
    (if (and res (string-equal (cdr res) "hankaku_kana"))
	(if anthy-use-hankaku-kana res nil)
      res)))

(defun anthy-handle-normal-key (chenc)
  (let* ((repl
	  (if chenc (anthy-send-recv-command 
		     (concat chenc "\n"))
	    nil)))
    (if repl
	(anthy-proc-agent-reply repl))))

(defun anthy-handle-enum-candidate-mode (chenc)
  (anthy-handle-normal-key chenc))

;;
(defun anthy-insert (&optional arg)
  "Anthy$B$N%-!<%O%s%I%i(B"
  (interactive "*p")
  ;; suzuki : last-command-char $B$r(B (anthy-last-command-char) $B$KJQ99(B
  (let* ((ch (anthy-last-command-char))
	 (chenc (anthy-encode-key ch)))
    (anthy-handle-key ch chenc)))

(defun anthy-handle-key (ch chenc)
  (cond
   ;; $B8uJdA*Br%b!<%I$+$i8uJd$rA*$V(B
   ((and (or anthy-enum-candidate-p anthy-enum-rcandidate-p)
	 (integerp ch)
	 (assq (car (rassoc (char-to-string ch)
			    anthy-select-candidate-keybind))
	       anthy-enum-candidate-list))
    (anthy-insert-select-candidate ch))
   ;; $B%-!<%^%C%W$rJQ99$9$k%3%^%s%I$r=hM}$9$k(B
   ((and (anthy-find-rkmap-keybind ch)
	 (string-equal anthy-preedit ""))
    (let ((mapname (cdr (anthy-find-rkmap-keybind ch))))
      (let ((repl (anthy-send-recv-command
		   (concat " MAP_SELECT " mapname "\n"))))
	(if (eq repl 'OK)
	    (progn
	      (setq anthy-current-rkmap
		    (cdr (assoc (cons anthy-current-rkmap ch)
				anthy-rkmap-keybind)))
	      (anthy-update-mode-line))))))
   ;; $B%"%k%U%!%Y%C%H%b!<%I$N>l9g$OD>@\F~NO(B
   ((and (string-equal anthy-current-rkmap "alphabet")
	 (string-equal anthy-preedit ""))
    (self-insert-command 1))
   ;; $B%W%j%(%G%#%C%H$,$J$/$F%9%Z!<%9$,2!$5$l$?(B
   ((and
     (string-equal anthy-preedit "")
     (= ch 32)
     (not
      (string-equal anthy-current-rkmap "alphabet")))
    (progn
      (insert-and-inherit anthy-wide-space)
      (anthy-do-auto-fill)))
   ((or anthy-enum-candidate-p anthy-enum-rcandidate-p)
    (anthy-handle-enum-candidate-mode chenc))
   ;; $BIaDL$NF~NO(B
   (t
    (anthy-handle-normal-key chenc))))

;;
(defun anthy-do-invoke-agent (cmd)
  (if (and (stringp anthy-personality)
	   (> (length anthy-personality) 0))
      (start-process "anthy-agent"
		     anthy-working-buffer
		     cmd
		     (concat " --personality=" anthy-personality))
    (start-process "anthy-agent"
		   anthy-working-buffer
		   cmd)))
;;
(defun anthy-invoke-agent ()
  (let ((list anthy-agent-command-list)
	(proc nil))
    (while (and list (not proc))
      (setq proc 
	    (anthy-do-invoke-agent (car list)))
      (if (not (boundp 'proc))
	  (setq proc nil))
      (setq list (cdr list)))
    proc))
;;
;;
;;
(defun anthy-check-agent ()
  ;; check and do invoke
  (if (not anthy-agent-process)
      (let
	  ((proc (anthy-invoke-agent)))
	(if anthy-agent-process
	    (kill-process anthy-agent-process))
	(setq anthy-agent-process proc)
	(process-kill-without-query proc)
	(if anthy-xemacs
	    (if (coding-system-p (find-coding-system 'euc-japan))
		(set-process-coding-system proc 'euc-japan 'euc-japan))
	  (cond ((coding-system-p 'euc-japan)
		 (set-process-coding-system proc 'euc-japan 'euc-japan))
		((coding-system-p '*euc-japan*)
		 (set-process-coding-system proc '*euc-japan* '*euc-japan*))))
	(set-process-sentinel proc 'anthy-process-sentinel))))
;;
(defun anthy-do-send-recv-command (cmd)
  (if (not anthy-agent-process)
      (anthy-check-agent))
  (let ((old-buffer (current-buffer)))
    (unwind-protect
	(progn
	  (set-buffer anthy-working-buffer)
	  (erase-buffer)
	  (process-send-string anthy-agent-process cmd)
	  (while (= (buffer-size) 0)
	    (accept-process-output nil 0 anthy-accept-timeout))
	  (read (buffer-string)))
      (set-buffer old-buffer))))
;;
(defun anthy-send-recv-command (cmd)
  (if anthy-context-id
      (anthy-do-send-recv-command
       (concat " SELECT_CONTEXT "
	       (number-to-string anthy-context-id)
	       "\n")))
  (anthy-do-send-recv-command cmd))
;;
(defun anthy-minibuffer-enter ()
  (setq anthy-saved-mode anthy-mode)
  (setq anthy-mode nil)
  (setq anthy-enable-enum-candidate-p 
	(cons nil anthy-enable-enum-candidate-p))
  (anthy-update-mode))
;;
(defun anthy-minibuffer-exit ()
  (setq anthy-mode anthy-saved-mode)
  (setq anthy-enable-enum-candidate-p 
	(cdr anthy-enable-enum-candidate-p))
  (anthy-update-mode))
;;
(defun anthy-kill-buffer ()
  (if anthy-context-id
      (anthy-send-recv-command
       " RELEASE_CONTEXT\n")))
;;
(defun anthy-mode-on ()
  (add-hook 'minibuffer-setup-hook 'anthy-minibuffer-enter)
  (add-hook 'minibuffer-exit-hook 'anthy-minibuffer-exit)
  (add-hook 'kill-buffer-hook 'anthy-kill-buffer)
  (anthy-check-context-id)
  (setq anthy-minor-mode t)
  (anthy-update-mode-line))
;;
(defun anthy-mode-off ()
  (setq anthy-minor-mode nil)
  (anthy-update-mode-line))
;;
(defun anthy-update-mode ()
  (if (or anthy-mode anthy-leim-active-p)
      (progn
	(anthy-check-agent)
	(anthy-mode-on))
    (anthy-mode-off)))

(defun anthy-mode (&optional arg)
  "Start Anthy conversion system."
  (interactive "P")
  (setq anthy-mode
        (if (null arg)
            (not anthy-mode)
          (> (prefix-numeric-value arg) 0)))
  (anthy-update-mode))

(defun anthy-select-map (map)
  (anthy-send-recv-command (concat " MAP_SELECT " map "\n"))
  (setq anthy-current-rkmap map)
  (anthy-update-mode-line))
;;
(defun anthy-hiragana-map (&optional arg)
  "Hiragana mode"
  (interactive "P")
  (anthy-select-map "hiragana"))
;;
(defun anthy-katakana-map (&optional arg)
  "Katakana mode"
  (interactive "P")
  (anthy-select-map "katakana"))
;;
(defun anthy-alpha-map (arg)
  "Alphabet mode"
  (interactive "P")
  (anthy-select-map "alphabet"))
;;
(defun anthy-wide-alpha-map (arg)
  "Wide Alphabet mode"
  (interactive "P")
  (anthy-select-map "walphabet"))
;;
(defun anthy-hankaku-kana-map (arg)
  "Hankaku Katakana mode"
  (interactive "P")
  (anthy-select-map "hankaku_kana"))
;;
;;
;; leim $B$N(B inactivate
;;
(defun anthy-leim-inactivate ()
  (setq anthy-leim-active-p nil)
  (anthy-update-mode))
;;
;; leim $B$N(B activate
;;
(defun anthy-leim-activate (&optional name)
  (setq inactivate-current-input-method-function 'anthy-leim-inactivate)
  (setq anthy-leim-active-p t)
  (anthy-update-mode)
  (when (eq (selected-window) (minibuffer-window))
    (add-hook 'minibuffer-exit-hook 'anthy-leim-exit-from-minibuffer)))

;;
;; emacs$B$N%P%0Hr$1$i$7$$$G$9(B
;;
(defun anthy-leim-exit-from-minibuffer ()
  (inactivate-input-method)
  (when (<= (minibuffer-depth) 1)
    (remove-hook 'minibuffer-exit-hook 'anthy-leim-exit-from-minibuffer)))

;;
;; Emacs / XEmacs $B%3%s%Q%A%V%k$J(B last-command-char
;; suzuki : $B?7@_(B
;;
(defun anthy-last-command-char ()
  "$B:G8e$NF~NO%$%Y%s%H$rJV$9!#(BXEmacs $B$G$O(B int $B$KJQ49$9$k(B"
  (if anthy-xemacs
      (let ((event last-command-event))
	(cond
	 ((event-matches-key-specifier-p event 'left)      2)
	 ((event-matches-key-specifier-p event 'right)     6)
	 ((event-matches-key-specifier-p event 'backspace) 8)
	 (t
	  (char-to-int (event-to-character event)))))
    last-command-char))

;;
;;
;;
;(global-set-key [(meta escape)] 'anthy-mode)
(provide 'anthy)

(require 'anthy-dic)
(require 'anthy-conf)

;; is it ok for i18n?
(set-language-info "Japanese" 'input-method "japanese-anthy")
(if (equal current-language-environment "Japanese")
    (progn
      (if (boundp 'default-input-method)
	  (setq-default default-input-method "japanese-anthy"))
      (setq default-input-method "japanese-anthy")))

(defun anthy-default-mode ()
  (interactive)
  (setq anthy-rkmap-keybind anthy-default-rkmap-keybind)
  (anthy-send-recv-command " MAP_CLEAR 1\n")
  (anthy-send-recv-command " SET_PREEDIT_MODE 0\n")
  (anthy-hiragana-map))

;;;
;;; anthy.el ends here
