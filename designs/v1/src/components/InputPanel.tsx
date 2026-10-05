import CodeMirror from '@uiw/react-codemirror';
import { cpp } from '@codemirror/lang-cpp';
import { EditorView } from '@codemirror/view';
import { HighlightStyle, syntaxHighlighting } from '@codemirror/language';
import { tags as t } from '@lezer/highlight';
import { FolderOpen, Save, Copy, ClipboardPaste, FileCode2 } from 'lucide-react';
import FaustLogo from '../design-system/icons/FaustLogo';

interface InputPanelProps {
  source: string;
  onSourceChange: (next: string) => void;
}

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
    '.cm-content': { backgroundColor: 'transparent' },
    '.cm-content': { caretColor: 'var(--color-text-primary)', padding: '8px 0' },
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

const InputPanel = ({ source, onSourceChange }: InputPanelProps) => (
  <div className="flex flex-col">
    <div className="flex items-center gap-2 px-4 py-2 bg-elevated border-b border-border">
      <span className="type-label">INPUT</span>
      <div className="ml-auto flex items-center gap-1">
        <ToolbarButton label="COPY CONTEXT" icon={FileCode2} />
        <ToolbarButton label="OPEN" icon={FolderOpen} />
        <ToolbarButton label="SAVE" icon={Save} />
        <ToolbarButton label="COPY" icon={Copy} />
        <ToolbarButton label="PASTE" icon={ClipboardPaste } />
      </div>
    </div>
    <div className="bg-surface relative overflow-hidden">
      <CodeMirror
        value={source}
        onChange={onSourceChange}
        extensions={[cpp(), editorTheme, syntaxHighlighting(editorHighlight)]}
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

export default InputPanel;
