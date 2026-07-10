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
import PillButton from '../design-system/components/PillButton';
import {
  EDITOR_SPECULAR,
  GlassScreenLayers,
  METAL_BAND_GRADIENT,
  MetalOverlay,
  SCREEN_BG_SHADOW,
} from '../design-system/chrome';
import MeterRail from './MeterRail';
import {
  isJuceHost,
  notifyEditorBufferChange,
  requestCompileBuffer,
  requestCopyContext,
  requestCopyToClipboard,
  requestOpen,
  requestPaste,
  requestSaveAs,
  subscribeClipboardPaste,
  useCompileResult,
  useInputErrors,
  useSourceChanged,
  useVariant,
} from '../lib/crucible-bridge';

const SELECTION = 'rgba(224,101,60,0.3)';

const editorTheme = EditorView.theme(
  {
    '&, &.cm-editor': {
      backgroundColor: 'transparent',
      color: 'var(--color-text-primary)',
      fontSize: '12px',
      fontFamily: 'var(--font-mono)',
      height: '100%',
    },
    '.cm-scroller': {
      backgroundColor: 'transparent',
      fontFamily: 'var(--font-mono)',
      lineHeight: '1.6',
      // The scroller is a flex item in the flex-column .cm-editor; it
      // defaults to flex-grow:0 and shrinks to content height, leaving
      // the empty lower area outside the editable/clickable surface.
      // Grow it to fill the fixed-height editor (minHeight:0 lets it
      // shrink for internal scrolling when the doc overflows). Override
      // CM's base `align-items: flex-start !important` so the content
      // stretches to the full height and the whole surface is clickable.
      flexGrow: 1,
      minHeight: 0,
      alignItems: 'stretch !important',
    },
    '.cm-content': {
      backgroundColor: 'transparent',
      caretColor: 'var(--color-accent-primary)',
      padding: '8px 0',
      // Fill the fixed-height screen so the whole surface is editable
      // and clickable even when the document is empty or short.
      minHeight: '100%',
      textShadow: '0 0 7px color-mix(in srgb, var(--color-accent-primary) 22%, transparent), 0 0 4px rgba(232,227,219,0.28)',
    },
    '.cm-line': { padding: '0 12px' },
    '.cm-gutters': {
      backgroundColor: 'transparent',
      color: 'var(--color-gutter)',
      borderRight: '1px solid #241F19',
      paddingRight: '4px',
    },
    '.cm-lineNumbers .cm-gutterElement': {
      color: 'var(--color-gutter)',
      padding: '0 4px 0 10px',
      minWidth: '2ch',
      textAlign: 'right',
    },
    '.cm-activeLine': { backgroundColor: 'transparent' },
    '.cm-activeLineGutter': { backgroundColor: 'transparent', color: 'var(--color-text-secondary)' },
    '.cm-selectionBackground, ::selection': { backgroundColor: `${SELECTION} !important` },
    '&.cm-focused .cm-selectionBackground, .cm-line ::selection': { backgroundColor: `${SELECTION} !important` },
    '&.cm-focused': { outline: 'none' },
    '.cm-cursor, .cm-dropCursor': {
      borderLeftColor: 'var(--color-accent-primary)',
      borderLeftWidth: '2px',
    },
    '.cm-matchingBracket, .cm-nonmatchingBracket': {
      backgroundColor: 'transparent',
      outline: '1px solid var(--color-text-secondary)',
    },
    '.cm-searchMatch': { backgroundColor: 'var(--color-accent-primary)', color: 'var(--color-screen)' },
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
    '.cm-scroller::-webkit-scrollbar': { width: '14px', height: '14px' },
    '.cm-scroller::-webkit-scrollbar-track': { background: '#090806' },
    '.cm-scroller::-webkit-scrollbar-track:horizontal': { boxShadow: 'inset 0 1px 0 0 #2A2620' },
    '.cm-scroller::-webkit-scrollbar-track:vertical': { boxShadow: 'inset 1px 0 0 0 #2A2620' },
    '.cm-scroller::-webkit-scrollbar-thumb': { background: '#3A3632', borderRadius: '1px' },
    '.cm-scroller::-webkit-scrollbar-thumb:hover': { background: '#5A5550' },
    '.cm-scroller::-webkit-scrollbar-corner': { background: '#090806' },
  },
  { dark: true },
);

const editorHighlight = HighlightStyle.define([
  { tag: t.keyword, color: 'var(--color-text-primary)' },
  { tag: [t.string, t.special(t.string)], color: 'var(--color-unit-2)' },
  { tag: [t.number, t.bool, t.null], color: 'var(--color-unit-1)' },
  { tag: t.comment, color: 'var(--color-dimmed)', fontStyle: 'italic' },
  { tag: [t.function(t.variableName), t.function(t.propertyName)], color: 'var(--color-accent-secondary)' },
  { tag: t.operator, color: 'var(--color-text-secondary)' },
  { tag: t.variableName, color: 'var(--color-text-primary)' },
  { tag: t.propertyName, color: 'var(--color-text-primary)' },
  { tag: t.punctuation, color: 'var(--color-text-secondary)' },
]);

const InputPanel = () => {
  const variant = useVariant();
  const showInputRail = variant === 'fx';
  const sourceEvent = useSourceChanged();
  const compileResult = useCompileResult();
  const inputErrors = useInputErrors();
  const editorRef = useRef<ReactCodeMirrorRef>(null);
  const localBufferRef = useRef<string>(sourceEvent?.source ?? '');
  const compileDebounceRef = useRef<number | null>(null);

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
    linter(() => []),
    // Wrap long lines to the editor width — the plugin window is a fixed
    // size and must never scroll horizontally (layout-stability rule).
    EditorView.lineWrapping,
    // Paste overwrites the whole buffer: this editor holds one Faust
    // program, so a paste is "load this program", not "insert a snippet".
    EditorView.domEventHandlers({
      paste: (event, view) => {
        const text = event.clipboardData?.getData('text') ?? '';
        view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: text } });
        event.preventDefault();
        return true;
      },
    }),
  ], []);

  const errorIconVisible = inputErrors.length > 0;
  const tooltipMessage = errorIconVisible
    ? inputErrors[inputErrors.length - 1]?.message ?? ''
    : '';

  // Overwrite the whole editor buffer. When the view exists its dispatch
  // fires onChange (→ notify + compile); the fallback covers the rare
  // pre-mount case.
  const overwriteEditor = useCallback((text: string) => {
    const view = editorRef.current?.view;
    if (view) {
      view.dispatch({ changes: { from: 0, to: view.state.doc.length, insert: text } });
    } else {
      notifyEditorBufferChange(text);
      scheduleCompile(text);
    }
  }, [scheduleCompile]);

  // C++ replies to a paste request through this channel.
  useEffect(() => subscribeClipboardPaste(overwriteEditor), [overwriteEditor]);

  const onCopy = useCallback(() => {
    if (isJuceHost()) { requestCopyToClipboard(localBufferRef.current); return; }
    void navigator.clipboard.writeText(localBufferRef.current);
  }, []);

  const onPaste = useCallback(() => {
    if (isJuceHost()) { requestPaste(); return; }
    void navigator.clipboard.readText().then(overwriteEditor);
  }, [overwriteEditor]);

  const onSave = useCallback(() => { requestSaveAs(localBufferRef.current); }, []);

  return (
    <>
      <div
        className="relative flex items-center overflow-hidden"
        style={{
          height: 38,
          gap: 8,
          padding: '0 16px',
          background: METAL_BAND_GRADIENT,
          boxShadow: 'inset 0 1px 0 rgba(255,255,255,0.04)',
          zIndex: 0,
        }}
      >
        <MetalOverlay />
        <span style={{ fontSize: 10, textTransform: 'uppercase', letterSpacing: '0.06em', color: 'var(--color-text-secondary)' }}>
          INPUT
        </span>
        {errorIconVisible && (
          <span
            title={tooltipMessage}
            aria-label={`Input error: ${tooltipMessage}`}
            className="inline-flex items-center"
            style={{ color: 'var(--color-accent-primary)', filter: 'drop-shadow(0 0 4px color-mix(in srgb, var(--color-accent-primary) 60%, transparent))' }}
          >
            <AlertTriangle size={12} strokeWidth={1.75} />
          </span>
        )}
        <div className="ml-auto flex items-center" style={{ gap: 5 }}>
          <PillButton label="COPY CONTEXT" icon={FileCode2} onClick={requestCopyContext} />
          <PillButton label="OPEN" icon={FolderOpen} onClick={requestOpen} />
          <PillButton label="SAVE" icon={Save} onClick={onSave} />
          <PillButton label="COPY" icon={Copy} onClick={onCopy} />
          <PillButton label="PASTE" icon={ClipboardPaste} onClick={onPaste} />
        </div>
      </div>

      <div className="flex items-stretch" style={{ height: 296 }}>
        {showInputRail && <MeterRail side="in" />}
        <div
          className="relative flex-1 min-w-0 overflow-hidden"
          style={{ background: 'var(--color-screen)', boxShadow: SCREEN_BG_SHADOW, zIndex: 0 }}
        >
          <GlassScreenLayers specular={EDITOR_SPECULAR} />
          <div className="relative h-full" style={{ zIndex: 1 }}>
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
              height="296px"
              minHeight="296px"
              maxHeight="296px"
            />
          </div>
          <a
            href="https://faustdoc.grame.fr/manual/quick-start/"
            target="_blank"
            rel="noopener noreferrer"
            aria-label="Faust documentation"
            className="absolute cursor-pointer"
            style={{ right: 16, bottom: 12, zIndex: 4, opacity: 0.3, lineHeight: 0, color: 'var(--color-text-secondary)' }}
          >
            <FaustLogo height={24} />
          </a>
        </div>
        <MeterRail side="out" />
      </div>
    </>
  );
};

export default InputPanel;
