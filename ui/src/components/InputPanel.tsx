import { useCallback, useEffect, useMemo, useRef } from 'react';
import CodeMirror from '@uiw/react-codemirror';
import type { ReactCodeMirrorRef } from '@uiw/react-codemirror';
import { cpp } from '@codemirror/lang-cpp';
import { EditorView } from '@codemirror/view';
import { HighlightStyle, syntaxHighlighting } from '@codemirror/language';
import { linter, lintGutter, setDiagnostics } from '@codemirror/lint';
import type { Diagnostic } from '@codemirror/lint';
import { tags as t } from '@lezer/highlight';
import { AlertTriangle, FolderOpen, Save, Copy, ClipboardPaste, FileCode2 } from 'lucide-react';
import FaustLogo from '../design-system/icons/FaustLogo';
import {
  notifyEditorBufferChange,
  requestCompileBuffer,
  requestCopyContext,
  requestOpen,
  requestSaveAs,
  useCompileResult,
  useInputErrors,
  useSourceChanged,
} from '../lib/crucible-bridge';

const editorTheme = EditorView.theme(
  {
    '&, &.cm-editor': {
      backgroundColor: 'transparent',
      color: 'var(--color-text-primary)',
      fontSize: '12px',
      fontFamily: 'var(--font-mono)',
    },
    '.cm-scroller': {
      backgroundColor: 'transparent',
      fontFamily: 'var(--font-mono)',
      lineHeight: '1.6',
    },
    '.cm-content': { backgroundColor: 'transparent', caretColor: 'var(--color-text-primary)', padding: '8px 0' },
    '.cm-line': { padding: '0 12px' },
    '.cm-gutters': {
      backgroundColor: 'var(--color-surface)',
      color: 'var(--color-dimmed)',
      borderRight: '1px solid var(--color-border)',
      paddingRight: '4px',
    },
    '.cm-lineNumbers .cm-gutterElement': {
      color: 'var(--color-dimmed)',
      padding: '0 4px 0 8px',
      minWidth: '2ch',
      textAlign: 'right',
    },
    '.cm-activeLine': { backgroundColor: 'transparent' },
    '.cm-activeLineGutter': { backgroundColor: 'transparent', color: 'var(--color-text-secondary)' },
    '.cm-selectionBackground, ::selection': { backgroundColor: 'var(--color-border) !important' },
    '&.cm-focused .cm-selectionBackground, .cm-line ::selection': {
      backgroundColor: 'var(--color-border) !important',
    },
    '&.cm-focused': { outline: 'none' },
    '.cm-cursor, .cm-dropCursor': {
      borderLeftColor: 'var(--color-accent-primary)',
      borderLeftWidth: '2px',
    },
    '.cm-matchingBracket, .cm-nonmatchingBracket': {
      backgroundColor: 'var(--color-border)',
      outline: '1px solid var(--color-text-secondary)',
    },
    '.cm-searchMatch': { backgroundColor: 'var(--color-accent-primary)', color: 'var(--color-surface)' },
    '.cm-panels': {
      backgroundColor: 'var(--color-elevated)',
      color: 'var(--color-text-primary)',
      borderTop: '1px solid var(--color-border)',
    },
    '.cm-tooltip': {
      backgroundColor: 'var(--color-elevated)',
      color: 'var(--color-text-primary)',
      border: '1px solid var(--color-border)',
    },
    '.cm-scroller::-webkit-scrollbar': { width: '16px', height: '16px' },
    '.cm-scroller::-webkit-scrollbar-track': { background: 'var(--color-surface)' },
    '.cm-scroller::-webkit-scrollbar-track:horizontal': {
      boxShadow: 'inset 0 1px 0 0 var(--color-border)',
    },
    '.cm-scroller::-webkit-scrollbar-track:vertical': {
      boxShadow: 'inset 1px 0 0 0 var(--color-border)',
    },
    '.cm-scroller::-webkit-scrollbar-thumb': { background: 'var(--color-dimmed)' },
    '.cm-scroller::-webkit-scrollbar-thumb:hover': { background: 'var(--color-text-secondary)' },
    '.cm-scroller::-webkit-scrollbar-corner': { background: 'var(--color-surface)' },
  },
  { dark: true },
);

const editorHighlight = HighlightStyle.define([
  { tag: t.keyword, color: 'var(--color-accent-primary)' },
  { tag: [t.string, t.special(t.string)], color: 'var(--color-unit-2)' },
  { tag: [t.number, t.bool, t.null], color: 'var(--color-unit-1)' },
  { tag: t.comment, color: 'var(--color-dimmed)', fontStyle: 'italic' },
  { tag: [t.function(t.variableName), t.function(t.propertyName)], color: 'var(--color-accent-secondary)' },
  { tag: t.operator, color: 'var(--color-text-secondary)' },
  { tag: t.variableName, color: 'var(--color-text-primary)' },
  { tag: t.propertyName, color: 'var(--color-text-primary)' },
  { tag: t.punctuation, color: 'var(--color-text-secondary)' },
]);

const ToolbarButton = ({ label, icon: Icon, onClick }: { label: string; icon: typeof Save; onClick?: () => void }) => (
  <button
    type="button"
    onClick={onClick}
    aria-label={label}
    className="flex items-center gap-1 px-2 py-1 bg-transparent border border-border text-text-secondary hover:text-text-primary hover:border-text-secondary transition-colors cursor-pointer text-[10px] uppercase tracking-[0.06em]"
  >
    <Icon size={12} strokeWidth={1.5} />
    <span>{label}</span>
  </button>
);

const InputPanel = () => {
  const sourceEvent = useSourceChanged();
  const compileResult = useCompileResult();
  const inputErrors = useInputErrors();
  const editorRef = useRef<ReactCodeMirrorRef>(null);
  // Tracks the latest buffer the user has typed locally so SAVE / COPY
  // round-trip the unsaved edits. Initialised from the bridge's source
  // event, replaced on every keystroke.
  const localBufferRef = useRef<string>(sourceEvent?.source ?? '');
  // Debounce handle for the in-memory compile trigger. Reset on every
  // keystroke; fires `requestCompileBuffer` after 400 ms of editor idle.
  // Pairs with `notifyEditorBufferChange` (which fires every keystroke
  // so SAVE-As / close-reopen restore see the current buffer).
  const compileDebounceRef = useRef<number | null>(null);

  // The CodeMirror `value` prop drives the editor when external file
  // writes overwrite the buffer (e.g. external editor saves while
  // Crucible is open). Local typing updates `localBufferRef` but does
  // not bump `value`; CodeMirror handles its own state.
  const editorValue = sourceEvent?.source ?? '';

  useEffect(() => {
    localBufferRef.current = editorValue;
  }, [editorValue]);

  const scheduleCompile = useCallback((next: string) => {
    if (compileDebounceRef.current !== null) {
      window.clearTimeout(compileDebounceRef.current);
    }
    compileDebounceRef.current = window.setTimeout(() => {
      compileDebounceRef.current = null;
      requestCompileBuffer(next);
    }, 400);
  }, []);

  const onChange = useCallback((next: string) => {
    localBufferRef.current = next;
    notifyEditorBufferChange(next);
    scheduleCompile(next);
  }, [scheduleCompile]);

  useEffect(() => () => {
    if (compileDebounceRef.current !== null) {
      window.clearTimeout(compileDebounceRef.current);
    }
  }, []);

  // Push lint diagnostics on every new compile result. Empty array on
  // success (clears markers); a single error diagnostic when the
  // libfaust message parsed to a line:col; nothing when it didn't.
  useEffect(() => {
    const view = editorRef.current?.view;
    if (!view) return;
    if (!compileResult) return;

    let diagnostics: Array<Diagnostic> = [];
    if (!compileResult.success
        && compileResult.errorLine !== undefined
        && !compileResult.crossLoad) {
      const lineNumber = Math.max(1, compileResult.errorLine);
      const docLines = view.state.doc.lines;
      const safeLine = Math.min(lineNumber, docLines);
      const line = view.state.doc.line(safeLine);
      const col = Math.max(0, compileResult.errorCol ?? 0);
      const from = Math.min(line.from + col, line.to);
      diagnostics = [{
        from,
        to: line.to,
        severity: 'error',
        source: 'libfaust',
        message: compileResult.errorMessage,
      }];
    }

    view.dispatch(setDiagnostics(view.state, diagnostics));
  }, [compileResult]);

  const extensions = useMemo(() => [
    cpp(),
    editorTheme,
    syntaxHighlighting(editorHighlight),
    lintGutter(),
    // `linter(() => [])` registers the linter plumbing without any
    // pull-mode diagnostics; we push via `setDiagnostics` instead.
    linter(() => []),
  ], []);

  const errorIconVisible = inputErrors.length > 0;
  const tooltipMessage = errorIconVisible
    ? inputErrors[inputErrors.length - 1]?.message ?? ''
    : '';

  const onCopy = useCallback(() => {
    void navigator.clipboard.writeText(localBufferRef.current);
  }, []);

  const onPaste = useCallback(() => {
    void navigator.clipboard.readText().then((text) => {
      // Update the editor buffer through the bridge so external
      // listeners (the dev cache, eventually the in-VST overwrite
      // mechanism) see the same string.
      notifyEditorBufferChange(text);
      const view = editorRef.current?.view;
      if (view) {
        view.dispatch({
          changes: { from: 0, to: view.state.doc.length, insert: text },
        });
      }
      // Dispatch above also triggers onChange (and thus scheduleCompile)
      // when the view is present. When no view exists yet, schedule
      // manually so the paste still compiles.
      if (!view) scheduleCompile(text);
    });
  }, [scheduleCompile]);

  const onSave = useCallback(() => { requestSaveAs(localBufferRef.current); }, []);

  return (
    <div className="flex flex-col">
      <div className="flex items-center gap-2 px-4 py-2 bg-elevated border-b border-border">
        <span className="type-label">INPUT</span>
        {errorIconVisible && (
          <span
            title={tooltipMessage}
            aria-label={`Input error: ${tooltipMessage}`}
            className="inline-flex items-center text-accent-primary"
          >
            <AlertTriangle size={12} strokeWidth={1.75} />
          </span>
        )}
        <div className="ml-auto flex items-center gap-1">
          <ToolbarButton label="COPY CONTEXT" icon={FileCode2} onClick={requestCopyContext} />
          <ToolbarButton label="OPEN" icon={FolderOpen} onClick={requestOpen} />
          <ToolbarButton label="SAVE" icon={Save} onClick={onSave} />
          <ToolbarButton label="COPY" icon={Copy} onClick={onCopy} />
          <ToolbarButton label="PASTE" icon={ClipboardPaste} onClick={onPaste} />
        </div>
      </div>
      <div className="bg-surface relative overflow-hidden">
        <CodeMirror
          ref={editorRef}
          value={editorValue}
          onChange={onChange}
          extensions={extensions}
          basicSetup={{
            lineNumbers: true,
            foldGutter: false,
            highlightActiveLine: false,
            highlightActiveLineGutter: false,
          }}
          height="300px"
          minHeight="300px"
          maxHeight="300px"
        />
        <a
          href="https://faustdoc.grame.fr/manual/quick-start/"
          target="_blank"
          rel="noopener noreferrer"
          aria-label="Faust documentation"
          className="absolute z-10 text-text-secondary opacity-30 cursor-pointer"
          style={{ right: 16, bottom: 12 }}
        >
          <FaustLogo height={24} />
        </a>
      </div>
    </div>
  );
};

export default InputPanel;
