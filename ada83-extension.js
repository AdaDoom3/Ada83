'use strict';

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const vscode = require('vscode');

const Header_Terminator = '\r\n\r\n';

const Content_Length_Of = (Header) =>
  ((Match) => (Match === null ? null : Number (Match[1])))
    (/content-length:\s*(\d+)/i.exec (Header));

const Framed = (Message) =>
  ((Body) =>
     `Content-Length: ${Buffer.byteLength (Body, 'utf8')}` +
     Header_Terminator + Body)
    (JSON.stringify ({ jsonrpc: '2.0', ...Message }));

const Unframed = (Stream) => {
  const Split = Stream.indexOf (Header_Terminator);
  if (Split < 0) return { Messages: [], Remainder: Stream };

  const Length = Content_Length_Of (Stream.slice (0, Split));
  const Start = Split + Header_Terminator.length;

  if (Length === null) return Unframed (Stream.slice (Start));
  if (Buffer.byteLength (Stream.slice (Start), 'utf8') < Length)
    return { Messages: [], Remainder: Stream };

  const Body = Stream.slice (Start, Start + Length);
  const Rest = Unframed (Stream.slice (Start + Length));
  return { Messages: [Body, ...Rest.Messages], Remainder: Rest.Remainder };
};

const Parsed = (Body) => {
  try { return JSON.parse (Body); } catch { return null; }
};

const Configured = (Name, Fallback) =>
  vscode.workspace.getConfiguration ('ada83').get (Name, Fallback);

const Opened_Folder = () =>
  ((Folders) =>
     (Folders === undefined || Folders.length === 0 ? null : Folders[0]))
    (vscode.workspace.workspaceFolders);

const Initialize = (Id) => ({
  id: Id,
  method: 'initialize',
  params: {
    processId: process.pid,
    rootUri: ((Folder) => (Folder === null ? null : Folder.uri.toString ()))
               (Opened_Folder ()),
    workspaceFolders:
      ((Folder) =>
         (Folder === null
            ? null
            : [{ uri: Folder.uri.toString (), name: Folder.name }]))
        (Opened_Folder ()),
    capabilities: {},
  },
});

const Initialized = () => ({ method: 'initialized', params: {} });

const Did_Open = (Document) => ({
  method: 'textDocument/didOpen',
  params: {
    textDocument: {
      uri: Document.uri.toString (),
      languageId: 'ada83',
      version: Document.version,
      text: Document.getText (),
    },
  },
});

const Did_Change = (Document) => ({
  method: 'textDocument/didChange',
  params: {
    textDocument: {
      uri: Document.uri.toString (),
      version: Document.version,
    },
    contentChanges: [{ text: Document.getText () }],
  },
});

const Did_Close = (Document) => ({
  method: 'textDocument/didClose',
  params: { textDocument: { uri: Document.uri.toString () } },
});

const Definition = (Id, Document, Position) => ({
  id: Id,
  method: 'textDocument/definition',
  params: {
    textDocument: { uri: Document.uri.toString () },
    position: { line: Position.line, character: Position.character },
  },
});

const Shutdown = () => ({ id: 2, method: 'shutdown' });

const Exit = () => ({ method: 'exit' });

const Severity_Table = {
  1: vscode.DiagnosticSeverity.Error,
  2: vscode.DiagnosticSeverity.Warning,
  3: vscode.DiagnosticSeverity.Information,
  4: vscode.DiagnosticSeverity.Hint,
};

const Severity_Of = (Reported) =>
  Severity_Table[Reported] ?? vscode.DiagnosticSeverity.Error;

const Range_Of = ({ start, end }) =>
  new vscode.Range (start.line, start.character, end.line, end.character);

const Related_Of = (Reported) =>
  new vscode.DiagnosticRelatedInformation (
    new vscode.Location (vscode.Uri.parse (Reported.location.uri),
                         Range_Of (Reported.location.range)),
    Reported.message ?? '');

const Diagnostic_Of = (Reported) =>
  Object.assign (
    new vscode.Diagnostic (Range_Of (Reported.range), Reported.message,
                           Severity_Of (Reported.severity)),
    { source: 'ada83',
      code: Reported.code,
      relatedInformation:
        (Reported.relatedInformation ?? []).map (Related_Of) });

const Location_Of = (Result) =>
  Result === null || Result === undefined
    ? null
    : new vscode.Location (vscode.Uri.parse (Result.uri),
                           Range_Of (Result.range));

const Is_Answer = (Message) =>
  Message !== null && Message.id !== undefined &&
  Message.method === undefined;

const Is_Publication = (Message) =>
  Message !== null && Message.method === 'textDocument/publishDiagnostics';

const Publication_Of = (Message) => ({
  Uri: vscode.Uri.parse (Message.params.uri),
  Diagnostics: (Message.params.diagnostics ?? []).map (Diagnostic_Of),
});

const Publications_In = (Messages) =>
  Messages.filter (Is_Publication).map (Publication_Of);

const Answers_In = (Messages) => Messages.filter (Is_Answer);

const Is_Ada_Document = (Document) =>
  Document.languageId === 'ada83' && Document.uri.scheme === 'file';

let Session = null;
let Next_Id = 10;
let Trace_Channel = null;
const Awaiting = new Map ();

const Traced = (Direction, Message) =>
  ((Level) => {
     if (Level === 'off' || Trace_Channel === null) return;
     Trace_Channel.appendLine (
       Level === 'verbose'
         ? `${Direction} ${JSON.stringify (Message)}`
         : `${Direction} ${Message.method ?? `id ${Message.id}`}`);
   }) (Configured ('trace.server', 'off'));

const Send = (Message) => {
  if (Session === null || !Session.Server.stdin.writable) return;
  Traced ('->', Message);
  Session.Server.stdin.write (Framed (Message));
};

const Settle = (Message) =>
  ((Resolve) => {
     if (Resolve === undefined) return;
     Traced ('<-', Message);
     Awaiting.delete (Message.id);
     Resolve (Message.result ?? null);
   }) (Awaiting.get (Message.id));

const Abandon_All = () =>
  [...Awaiting.entries ()].forEach (([Id, Resolve]) => {
    Awaiting.delete (Id);
    Resolve (null);
  });

const Receive = (Chunk) => {
  if (Session === null) return;
  const { Messages, Remainder } = Unframed (Session.Buffered + Chunk);
  Session.Buffered = Remainder;
  const Parsed_Messages = Messages.map (Parsed);
  Publications_In (Parsed_Messages).forEach (({ Uri, Diagnostics }) =>
    Session.Diagnostics.set (Uri, Diagnostics));
  Answers_In (Parsed_Messages).forEach (Settle);
};

let Pending_Change = null;

const Flush_Changes = () => {
  if (Pending_Change === null) return;
  const { Timer, Notify, Document } = Pending_Change;
  clearTimeout (Timer);
  Pending_Change = null;
  Send (Notify (Document));
};

const Cancel = (Id) => ({ method: '$/cancelRequest', params: { id: Id } });

const Ask = (Build, Token, Patience) =>
  Session === null
    ? Promise.resolve (null)
    : ((Id) =>
         new Promise ((Resolve) => {
           Awaiting.set (Id, Resolve);
           Flush_Changes ();
           Send (Build (Id));
           const Abandon = (Notify) => () => {
             if (!Awaiting.delete (Id)) return;
             if (Notify) Send (Cancel (Id));
             Resolve (null);
           };
           const Waiting = setTimeout (
             Abandon (true),
             Patience ?? Configured ('requestTimeout', 15000));
           Token?.onCancellationRequested (() => {
             clearTimeout (Waiting);
             Abandon (true) ();
           });
         })) (Next_Id++);

const Definition_Provider = {
  provideDefinition: (Document, Position, Token) =>
    Ask ((Id) => Definition (Id, Document, Position), Token)
      .then (Location_Of),
};

const Document_Highlight = (Id, Document, Position) => ({
  id: Id,
  method: 'textDocument/documentHighlight',
  params: {
    textDocument: { uri: Document.uri.toString () },
    position: { line: Position.line, character: Position.character },
  },
});

const References = (Id, Document, Position, Include_Declaration) => ({
  id: Id,
  method: 'textDocument/references',
  params: {
    textDocument: { uri: Document.uri.toString () },
    position: { line: Position.line, character: Position.character },
    context: { includeDeclaration: Include_Declaration },
  },
});

const Highlight_Kind_Table = {
  1: vscode.DocumentHighlightKind.Text,
  2: vscode.DocumentHighlightKind.Read,
  3: vscode.DocumentHighlightKind.Write,
};

const Highlight_Kind_Of = (Reported) =>
  Highlight_Kind_Table[Reported] ?? vscode.DocumentHighlightKind.Text;

const Highlight_Of = (Reported) =>
  new vscode.DocumentHighlight (Range_Of (Reported.range),
                                Highlight_Kind_Of (Reported.kind));

const Listed = (Convert) => (Result) =>
  Array.isArray (Result) ? Result.map (Convert) : [];

const Highlights_Of = Listed (Highlight_Of);

const Locations_Of = Listed (Location_Of);

const Highlight_Provider = {
  provideDocumentHighlights: (Document, Position, Token) =>
    Ask ((Id) => Document_Highlight (Id, Document, Position), Token)
      .then (Highlights_Of),
};

const Reference_Provider = {
  provideReferences: (Document, Position, Context, Token) =>
    Ask ((Id) => References (Id, Document, Position,
                             (Context ?? {}).includeDeclaration !== false),
         Token)
      .then (Locations_Of),
};

const Hover = (Id, Document, Position) => ({
  id: Id,
  method: 'textDocument/hover',
  params: {
    textDocument: { uri: Document.uri.toString () },
    position: { line: Position.line, character: Position.character },
  },
});

const Hover_Of = (Result) =>
  Result === null || Result === undefined ||
  Result.contents === null || Result.contents === undefined
    ? null
    : new vscode.Hover (new vscode.MarkdownString (Result.contents.value),
                        Range_Of (Result.range));

const Hover_Provider = {
  provideHover: (Document, Position, Token) =>
    Ask ((Id) => Hover (Id, Document, Position), Token).then (Hover_Of),
};

const Document_Symbols = (Id, Document) => ({
  id: Id,
  method: 'textDocument/documentSymbol',
  params: { textDocument: { uri: Document.uri.toString () } },
});

const Symbol_Kind_Of = (Reported) =>
  Number.isInteger (Reported) && Reported >= 1 && Reported <= 26
    ? Reported - 1
    : vscode.SymbolKind.Variable;

const Symbol_Of = (Reported) =>
  new vscode.SymbolInformation (Reported.name,
                                Symbol_Kind_Of (Reported.kind),
                                Reported.containerName ?? '',
                                Location_Of (Reported.location));

const Symbols_Of = (Result) =>
  (Array.isArray (Result) ? Result : []).map (Symbol_Of);

const Document_Symbol_Provider = {
  provideDocumentSymbols: (Document, Token) =>
    Ask ((Id) => Document_Symbols (Id, Document), Token).then (Symbols_Of),
};

const Completion = (Id, Document, Position) => ({
  id: Id,
  method: 'textDocument/completion',
  params: {
    textDocument: { uri: Document.uri.toString () },
    position: { line: Position.line, character: Position.character },
  },
});

const Completion_Kind_Table = {
  2: vscode.CompletionItemKind.Method,
  3: vscode.CompletionItemKind.Function,
  6: vscode.CompletionItemKind.Variable,
  9: vscode.CompletionItemKind.Module,
  14: vscode.CompletionItemKind.Keyword,
  20: vscode.CompletionItemKind.EnumMember,
  21: vscode.CompletionItemKind.Constant,
  22: vscode.CompletionItemKind.Struct,
  23: vscode.CompletionItemKind.Event,
};

const Completion_Kind_Of = (Reported) =>
  Completion_Kind_Table[Reported] ?? vscode.CompletionItemKind.Text;

const Completion_Item_Of = (Reported) =>
  Object.assign (
    new vscode.CompletionItem (Reported.label,
                               Completion_Kind_Of (Reported.kind)),
    { detail: Reported.detail });

const Completions_Of = (Result) =>
  Result === null || Result === undefined
    ? null
    : (Result.items ?? []).map (Completion_Item_Of);

const Completion_Provider = {
  provideCompletionItems: (Document, Position, Token) =>
    Ask ((Id) => Completion (Id, Document, Position), Token)
      .then (Completions_Of),
};

const Workspace_Symbols = (Id, Query) => ({
  id: Id,
  method: 'workspace/symbol',
  params: { query: Query },
});

const Workspace_Symbols_Of = Listed (Symbol_Of);

const Workspace_Symbol_Provider = {
  provideWorkspaceSymbols: (Query, Token) =>
    Ask ((Id) => Workspace_Symbols (Id, Query ?? ''), Token,
         Configured ('workspaceRequestTimeout', 120000))
      .then (Workspace_Symbols_Of),
};

const Signature_Help = (Id, Document, Position) => ({
  id: Id,
  method: 'textDocument/signatureHelp',
  params: {
    textDocument: { uri: Document.uri.toString () },
    position: { line: Position.line, character: Position.character },
  },
});

const Parameter_Of = (Reported) =>
  new vscode.ParameterInformation (Reported.label);

const Signature_Of = (Reported) =>
  Object.assign (new vscode.SignatureInformation (Reported.label),
                 { parameters: (Reported.parameters ?? []).map (Parameter_Of) });

const Signature_Help_Of = (Result) =>
  Result === null || Result === undefined
    ? null
    : Object.assign (new vscode.SignatureHelp (), {
        signatures: (Result.signatures ?? []).map (Signature_Of),
        activeSignature: Result.activeSignature ?? 0,
        activeParameter: Result.activeParameter ?? 0,
      });

const Signature_Help_Provider = {
  provideSignatureHelp: (Document, Position, Token) =>
    Ask ((Id) => Signature_Help (Id, Document, Position), Token)
      .then (Signature_Help_Of),
};

const Folding_Ranges = (Id, Document) => ({
  id: Id,
  method: 'textDocument/foldingRange',
  params: { textDocument: { uri: Document.uri.toString () } },
});

const Folding_Kind_Table = {
  comment: vscode.FoldingRangeKind.Comment,
  imports: vscode.FoldingRangeKind.Imports,
  region: vscode.FoldingRangeKind.Region,
};

const Folding_Range_Of = (Reported) =>
  new vscode.FoldingRange (Reported.startLine, Reported.endLine,
                           Folding_Kind_Table[Reported.kind]);

const Folding_Ranges_Of = Listed (Folding_Range_Of);

const Folding_Range_Provider = {
  provideFoldingRanges: (Document, Context, Token) =>
    Ask ((Id) => Folding_Ranges (Id, Document), Token)
      .then (Folding_Ranges_Of),
};

const Code_Actions = (Id, Document, Selection) => ({
  id: Id,
  method: 'textDocument/codeAction',
  params: {
    textDocument: { uri: Document.uri.toString () },
    range: {
      start: { line: Selection.start.line,
               character: Selection.start.character },
      end: { line: Selection.end.line, character: Selection.end.character },
    },
    context: { diagnostics: [] },
  },
});

const Edited = (Edit, Uri, Replacements) => {
  Replacements.forEach ((Replacement) =>
    Edit.replace (vscode.Uri.parse (Uri), Range_Of (Replacement.range),
                  Replacement.newText));
  return Edit;
};

const Workspace_Edit_Of = (Changes) =>
  Object.keys (Changes ?? {}).reduce (
    (Edit, Uri) => Edited (Edit, Uri, Changes[Uri]),
    new vscode.WorkspaceEdit ());

const Code_Action_Of = (Reported) =>
  Object.assign (
    new vscode.CodeAction (Reported.title, vscode.CodeActionKind.QuickFix),
    { edit: Workspace_Edit_Of ((Reported.edit ?? {}).changes),
      isPreferred: Reported.isPreferred === true });

const Code_Actions_Of = Listed (Code_Action_Of);

const Code_Action_Provider = {
  provideCodeActions: (Document, Selection, Context, Token) =>
    Ask ((Id) => Code_Actions (Id, Document, Selection), Token)
      .then (Code_Actions_Of),
};

const Manual_Name = 'manual.md';
const Manual_Results = 4;
const Manual_Section_Words = 700;
const Manual_Total_Words = 2400;
const Manual_Saturation = 1.2;
const Manual_Balance = 0.75;
const Clause_Bonus = 100;
const Title_Bonus = 2.5;

const Any_Heading = /^#{1,6}\s+\S/;
const Numbered_Heading =
  /^#{2,6}\s+([0-9]+(?:\.[0-9]+)*|[A-Z])\.?\s+(\S.*?)\s*$/;
const Word_Pattern = /[a-z0-9_]+/g;
const Clause_Pattern = /\b[0-9]+(?:\.[0-9]+)+\b/g;
const Clause_Query = /\b[0-9]+(?:\.[0-9]+)*\b/g;

const Heading_Of = (Line) =>
  ((Match) =>
     (Match === null ? null : { Clause: Match[1], Title: Match[2] }))
    (Numbered_Heading.exec (Line));

const Heading_Marks = (Lines) =>
  Lines.reduce ((Marks, Line, Index) =>
    (Any_Heading.test (Line) ? [...Marks, Index] : Marks), []);

const Sectioned = (Text) =>
  ((Lines) =>
     ((Marks) =>
        Marks
          .map ((Start, Position) => ({
            Heading: Heading_Of (Lines[Start]),
            Body: Lines
              .slice (Start + 1, Marks[Position + 1] ?? Lines.length)
              .join ('\n').trim (),
          }))
          .filter (({ Heading }) => Heading !== null)
          .map (({ Heading, Body }) => ({ ...Heading, Body })))
       (Heading_Marks (Lines)))
    (Text.split (/\r?\n/));

const Tokens_Of = (Text) =>
  ((Lowered) => [...(Lowered.match (Word_Pattern) ?? []),
                 ...(Lowered.match (Clause_Pattern) ?? [])])
    (Text.toLowerCase ());

const Counted = (Words) => {
  const Counts = new Map ();
  Words.forEach ((Word) => Counts.set (Word, (Counts.get (Word) ?? 0) + 1));
  return Counts;
};

const Entry_Of = (Section) =>
  ((Counts) => ({
     Section,
     Counts,
     Length: [...Counts.values ()].reduce ((Sum, Count) => Sum + Count, 0),
     Title_Terms: new Set (Tokens_Of (`${Section.Clause} ${Section.Title}`)),
   }))
    (Counted (Tokens_Of (
       `${Section.Clause} ${Section.Title}\n${Section.Body}`)));

const Document_Frequencies = (Entries) => {
  const Frequencies = new Map ();
  Entries.forEach (({ Counts }) =>
    Counts.forEach ((_, Word) =>
      Frequencies.set (Word, (Frequencies.get (Word) ?? 0) + 1)));
  return Frequencies;
};

const Indexed = (Sections) =>
  ((Entries) => ({
     Entries,
     Frequencies: Document_Frequencies (Entries),
     Count: Entries.length,
     Average_Length:
       Entries.reduce ((Sum, { Length }) => Sum + Length, 0) /
       Math.max (1, Entries.length),
   }))
    (Sections.map (Entry_Of));

const Inverse_Frequency = (Count, Documents) =>
  Math.log (1 + (Count - Documents + 0.5) / (Documents + 0.5));

const Term_Score = (Index, Entry, Word) =>
  ((Frequency, Documents) =>
     Frequency === 0 || Documents === 0
       ? 0
       : Inverse_Frequency (Index.Count, Documents) *
         (Frequency * (Manual_Saturation + 1)) /
         (Frequency + Manual_Saturation *
            (1 - Manual_Balance +
             Manual_Balance * Entry.Length / Index.Average_Length)))
    (Entry.Counts.get (Word) ?? 0, Index.Frequencies.get (Word) ?? 0);

const Score_Of = (Index, Entry, Words, Clauses) =>
  (Clauses.includes (Entry.Section.Clause) ? Clause_Bonus : 0) +
  Words.reduce ((Total, Word) =>
    Total + Term_Score (Index, Entry, Word) +
    (Entry.Title_Terms.has (Word) ? Title_Bonus : 0), 0);

const Ranked = (Index, Query) =>
  ((Words, Clauses) =>
     Index.Entries
       .map ((Entry) => ({
         Section: Entry.Section,
         Score: Score_Of (Index, Entry, Words, Clauses),
       }))
       .filter (({ Score }) => Score > 0)
       .sort ((Left, Right) => Right.Score - Left.Score)
       .slice (0, Manual_Results)
       .map (({ Section }) => Section))
    ([...new Set (Tokens_Of (Query))],
     Query.match (Clause_Query) ?? []);

const Words_In = (Text) => (Text.match (/\S+/g) ?? []).length;

const Running_Words = (Lines) => {
  let Total = 0;
  return Lines.map ((Line) => (Total += Words_In (Line)));
};

const Clipped = (Text, Limit) =>
  Limit <= 0
    ? ''
    : ((Lines) =>
         ((Cut) =>
            (Cut < 0
               ? Text
               : `${Lines.slice (0, Cut + 1).join ('\n')}\n[...]`))
           (Running_Words (Lines).findIndex ((Count) => Count >= Limit)))
        (Text.split ('\n'));

const Budgeted = (Sections) => {
  let Remaining = Manual_Total_Words;
  return Sections
    .map ((Section) =>
       ((Body) => {
          Remaining -= Words_In (Body);
          return { ...Section, Body };
        })
         (Clipped (Section.Body,
                   Math.min (Manual_Section_Words, Remaining))))
    .filter (({ Body }) => Body.length > 0);
};

const Rendered = (Sections, Query) =>
  Sections.length === 0
    ? `No section of the Ada 83 manual matches "${Query}".`
    : ['Ada 83 Reference Manual (ANSI/MIL-STD-1815A), sections matching ' +
       `"${Query}":`,
       ...Budgeted (Sections).map (({ Clause, Title, Body }) =>
         `## ${Clause} ${Title}\n\n${Body}`)].join ('\n\n');

const Manual_Text = (Root) => {
  try { return fs.readFileSync (path.join (Root, Manual_Name), 'utf8'); }
  catch { return null; }
};

let Manual_Cache;

const Manual_Index = (Root) => {
  if (Manual_Cache === undefined)
    Manual_Cache =
      ((Text) => (Text === null ? null : Indexed (Sectioned (Text))))
        (Manual_Text (Root));
  return Manual_Cache;
};

const Searched = (Root, Query) =>
  Query.trim () === ''
    ? 'Ask for a clause number, such as 4.5.7, or for the wording to find.'
    : ((Index) =>
         Index === null
           ? `The Ada 83 manual (${Manual_Name}) is not installed beside ` +
             'the extension.'
           : Rendered (Ranked (Index, Query), Query.trim ()))
        (Manual_Index (Root));

const Manual_Tool = (Root) => ({
  invoke: (Options) =>
    new vscode.LanguageModelToolResult ([
      new vscode.LanguageModelTextPart (
        Searched (Root, String (Options.input?.query ?? '')))]),
});

const Manual_Registration = (Root) =>
  vscode.lm === undefined || vscode.lm.registerTool === undefined
    ? []
    : [vscode.lm.registerTool ('search_ada83_manual', Manual_Tool (Root))];

const Ada_Selector = { scheme: 'file', language: 'ada83' };

const Task_Of = (Name, Title, Argument_List, Group) =>
  Object.assign (
    new vscode.Task (
      { type: 'ada83', task: Name },
      vscode.TaskScope.Workspace,
      Title,
      'ada83',
      new vscode.ShellExecution (Configured ('compilerPath', 'ada83'),
                                 Argument_List),
      '$ada83'),
    { group: Group });

const Build_Task = (Source_Path) =>
  Task_Of ('build', 'build this file', [Source_Path],
           vscode.TaskGroup.Build);

const Check_Task = (Source_Path) =>
  Task_Of ('check', 'check this file', ['--ir', Source_Path],
           vscode.TaskGroup.Test);

const Task_For = (Name) => (Name === 'check' ? Check_Task : Build_Task);

const Active_Ada_Path = () =>
  ((Editor) =>
     Editor === undefined || !Is_Ada_Document (Editor.document)
       ? null
       : Editor.document.uri.fsPath)
    (vscode.window.activeTextEditor);

const Tasks_For = (Source_Path) =>
  Source_Path === null
    ? []
    : [Build_Task (Source_Path), Check_Task (Source_Path)];

const Task_Provider = {
  provideTasks: () => Tasks_For (Active_Ada_Path ()),
  resolveTask: (Given) =>
    ((Source_Path) =>
       Source_Path === null
         ? undefined
         : Task_For (Given.definition.task) (Source_Path))
      (Active_Ada_Path ()),
};

const Run_On_The_Active_File = (Make) => () =>
  ((Source_Path) =>
     Source_Path === null
       ? vscode.window.showErrorMessage (
           'Ada 83: open an Ada file before building or checking it.')
       : vscode.tasks.executeTask (Make (Source_Path)))
    (Active_Ada_Path ());

const Substituted = (Setting) =>
  ((Folder) =>
     Setting.replace (/\$\{workspaceFolder\}/g,
                      Folder === null ? '' : Folder.uri.fsPath))
    (Opened_Folder ());

const Compiler_Path = () => Substituted (Configured ('compilerPath', 'ada83'));

const Presentation = {
  starting: ['$(loading~spin)', 'starting'],
  ready: ['$(check)', 'ready'],
  restarting: ['$(sync~spin)', 'restarting'],
  failed: ['$(error)', 'unavailable'],
  stopped: ['$(circle-slash)', 'stopped'],
};

const Show_State = (Status, State) => {
  const [Icon, Detail] = Presentation[State];
  Status.text = `${Icon} Ada 83`;
  Status.tooltip = `Ada 83 language server: ${Detail}`;
  Status.show ();
};

const Restart_Delay = (Attempt) => Math.min (1000 * 2 ** Attempt, 30000);
const Restart_Limit = 5;

const Start = (Diagnostics, Output, Status, Attempt) => {
  if (!Configured ('enable', true)) return Show_State (Status, 'stopped');

  const Command = Compiler_Path ();
  const Server = spawn (Command, ['--lsp'], { stdio: 'pipe' });
  const Ours = { Server, Diagnostics, Output, Status, Buffered: '',
                 Ready: false, Capabilities: {}, Registered: [] };
  Session = Ours;
  Show_State (Status, Attempt === 0 ? 'starting' : 'restarting');

  const Give_Up = (Reason) => {
    if (Session !== Ours) return;
    Session = null;
    Abandon_All ();
    Ours.Registered.forEach ((Registration) => Registration.dispose ());
    if (Attempt >= Restart_Limit) {
      Show_State (Status, 'failed');
      Output.appendLine (`server unavailable: ${Reason}`);
      return vscode.window.showErrorMessage (
        `Ada 83: '${Command} --lsp' will not run (${Reason}). Set ` +
        'ada83.compilerPath to the compiler, and keep ada83-runtime.ada ' +
        'beside it.');
    }
    Show_State (Status, 'restarting');
    Output.appendLine (`server ${Reason}; restarting`);
    setTimeout (() => {
      if (Session === null)
        Start (Diagnostics, Output, Status, Attempt + 1);
    }, Restart_Delay (Attempt));
  };

  Server.on ('error', (Reason) => Give_Up (Reason.message));
  Server.on ('exit', (Code, Signal) =>
    Give_Up (`exited with ${Signal ?? Code}`));
  Server.stdout.setEncoding ('utf8');
  Server.stdout.on ('data', Receive);
  Server.stderr.setEncoding ('utf8');
  Server.stderr.on ('data', (Text) => Output.append (Text));

  Ask (Initialize).then ((Result) => {
    if (Session !== Ours || Result === null) return;
    Ours.Ready = true;
    Ours.Capabilities = Result.capabilities ?? {};
    Ours.Registered = Registrations (Ours.Capabilities);
    Show_State (Status, 'ready');
    Send (Initialized ());
    vscode.workspace.textDocuments
      .filter (Is_Ada_Document)
      .map (Did_Open)
      .forEach (Send);
  });
};

const Stop = () => {
  if (Session === null) return;
  const Ending = Session;
  [Shutdown (), Exit ()].forEach (Send);
  Session = null;
  Abandon_All ();
  Ending.Registered.forEach ((Registration) => Registration.dispose ());
  Ending.Server.kill ();
};

const Restart = (Diagnostics, Output, Status) => {
  Stop ();
  Start (Diagnostics, Output, Status, 0);
};

const Registrations = (Capabilities) =>
  [['definitionProvider',
    () => vscode.languages.registerDefinitionProvider (
            Ada_Selector, Definition_Provider)],
   ['hoverProvider',
    () => vscode.languages.registerHoverProvider (
            Ada_Selector, Hover_Provider)],
   ['documentHighlightProvider',
    () => vscode.languages.registerDocumentHighlightProvider (
            Ada_Selector, Highlight_Provider)],
   ['referencesProvider',
    () => vscode.languages.registerReferenceProvider (
            Ada_Selector, Reference_Provider)],
   ['documentSymbolProvider',
    () => vscode.languages.registerDocumentSymbolProvider (
            Ada_Selector, Document_Symbol_Provider)],
   ['workspaceSymbolProvider',
    () => vscode.languages.registerWorkspaceSymbolProvider (
            Workspace_Symbol_Provider)],
   ['completionProvider',
    () => vscode.languages.registerCompletionItemProvider (
            Ada_Selector, Completion_Provider,
            ...(Capabilities.completionProvider?.triggerCharacters ?? ['.']))],
   ['signatureHelpProvider',
    () => vscode.languages.registerSignatureHelpProvider (
            Ada_Selector, Signature_Help_Provider,
            ...(Capabilities.signatureHelpProvider?.triggerCharacters
                  ?? ['(', ',']))],
   ['foldingRangeProvider',
    () => vscode.languages.registerFoldingRangeProvider (
            Ada_Selector, Folding_Range_Provider)],
   ['codeActionProvider',
    () => vscode.languages.registerCodeActionsProvider (
            Ada_Selector, Code_Action_Provider,
            { providedCodeActionKinds: [vscode.CodeActionKind.QuickFix] })]]
    .filter (([Name]) => Capabilities[Name] !== undefined &&
                         Capabilities[Name] !== false)
    .map (([, Register]) => Register ());

const On_Ada = (Notify) => (Event) =>
  ((Document) => { if (Is_Ada_Document (Document)) Send (Notify (Document)); })
    (Event.document ?? Event);

const Settles_Down = (Notify) => (Event) =>
  ((Document) => {
     if (!Is_Ada_Document (Document)) return;
     if (Pending_Change !== null) clearTimeout (Pending_Change.Timer);
     Pending_Change = {
       Notify,
       Document,
       Timer: setTimeout (Flush_Changes, Configured ('syncDelay', 120)),
     };
   }) (Event.document ?? Event);

const Touches_The_Server = (Change) =>
  ['enable', 'compilerPath', 'includePaths']
    .some ((Name) => Change.affectsConfiguration (`ada83.${Name}`));

const activate = (Context) => {
  const Output = vscode.window.createOutputChannel ('Ada 83');
  const Diagnostics = vscode.languages.createDiagnosticCollection ('ada83');
  const Status = vscode.window.createStatusBarItem (
    vscode.StatusBarAlignment.Right, 100);
  Status.command = 'ada83.showOutput';
  Trace_Channel = Output;

  Start (Diagnostics, Output, Status, 0);

  Context.subscriptions.push (
    Output,
    Diagnostics,
    Status,
    { dispose: Stop },
    vscode.workspace.onDidOpenTextDocument (On_Ada (Did_Open)),
    vscode.workspace.onDidChangeTextDocument (Settles_Down (Did_Change)),
    vscode.workspace.onDidCloseTextDocument (On_Ada (Did_Close)),
    vscode.workspace.onDidSaveTextDocument (On_Ada (Did_Change)),
    vscode.workspace.onDidChangeConfiguration ((Change) => {
      if (Touches_The_Server (Change)) Restart (Diagnostics, Output, Status);
    }),
    vscode.commands.registerCommand ('ada83.build',
                                     Run_On_The_Active_File (Build_Task)),
    vscode.commands.registerCommand ('ada83.check',
                                     Run_On_The_Active_File (Check_Task)),
    vscode.commands.registerCommand ('ada83.restartServer',
                                     () => Restart (Diagnostics, Output,
                                                    Status)),
    vscode.commands.registerCommand ('ada83.showOutput',
                                     () => Output.show (true)),
    vscode.tasks.registerTaskProvider ('ada83', Task_Provider),
    ...Manual_Registration (Context.extensionPath));
};

const deactivate = Stop;

module.exports = { activate, deactivate };

//= The rest of the extension travels here, one line comment per
//= line so that no payload can end the block it sits in: the
//= grammar contains */ and the snippets contain ${. `make vsix`
//= splits these back out into the shape VS Code installs.

//== package.json
//{
//  "name": "ada83",
//  "displayName": "Ada 83",
//  "description": "Ada 83 (ANSI/MIL-STD-1815A) support, with diagnostics from the ada83 compiler itself.",
//  "version": "1.0.0",
//  "publisher": "ada83",
//  "license": "SEE LICENSE IN ../readme.md",
//  "engines": {
//    "vscode": "^1.106.0"
//  },
//  "categories": [
//    "Programming Languages",
//    "Linters"
//  ],
//  "activationEvents": [
//    "onLanguage:ada83",
//    "onLanguageModelTool:search_ada83_manual"
//  ],
//  "main": "./ada83-extension.js",
//  "contributes": {
//    "languages": [
//      {
//        "id": "ada83",
//        "aliases": [
//          "Ada 83",
//          "ada"
//        ],
//        "extensions": [
//          ".ada",
//          ".adb",
//          ".ads"
//        ],
//        "configuration": "./language-configuration.json"
//      }
//    ],
//    "grammars": [
//      {
//        "language": "ada83",
//        "scopeName": "source.ada",
//        "path": "./syntaxes/ada83.tmLanguage.json"
//      }
//    ],
//    "snippets": [
//      {
//        "language": "ada83",
//        "path": "./snippets.json"
//      }
//    ],
//    "configuration": {
//      "title": "Ada 83",
//      "properties": {
//        "ada83.compilerPath": {
//          "type": "string",
//          "default": "ada83",
//          "description": "Path to the ada83 compiler. It is run as `ada83 --lsp`, and looks for ada83-runtime.ada beside itself."
//        },
//        "ada83.enable": {
//          "type": "boolean",
//          "default": true,
//          "description": "Report diagnostics from the compiler as you type."
//        },
//        "ada83.requestTimeout": {
//          "type": "number",
//          "default": 15000,
//          "description": "How long, in milliseconds, to wait for one answer from the language server. Every answer costs a compilation, so a large program needs a larger number."
//        },
//        "ada83.workspaceRequestTimeout": {
//          "type": "number",
//          "default": 120000,
//          "description": "How long, in milliseconds, to wait for a workspace-wide symbol search, which compiles every Ada source in the workspace root."
//        },
//        "ada83.syncDelay": {
//          "type": "number",
//          "default": 120,
//          "description": "How long, in milliseconds, typing must pause before the buffer is sent to the compiler. Every send costs a compilation."
//        },
//        "ada83.trace.server": {
//          "type": "string",
//          "enum": [
//            "off",
//            "messages",
//            "verbose"
//          ],
//          "default": "off",
//          "description": "Write the traffic between the editor and the language server to the Ada 83 output channel."
//        }
//      }
//    },
//    "commands": [
//      {
//        "command": "ada83.build",
//        "title": "Build This File",
//        "category": "Ada 83",
//        "icon": "$(gear)"
//      },
//      {
//        "command": "ada83.check",
//        "title": "Check This File",
//        "category": "Ada 83",
//        "icon": "$(check)"
//      },
//      {
//        "command": "ada83.restartServer",
//        "title": "Restart Language Server",
//        "category": "Ada 83",
//        "icon": "$(debug-restart)"
//      },
//      {
//        "command": "ada83.showOutput",
//        "title": "Show Server Log",
//        "category": "Ada 83",
//        "icon": "$(output)"
//      }
//    ],
//    "menus": {
//      "editor/title": [
//        {
//          "command": "ada83.build",
//          "when": "resourceLangId == ada83",
//          "group": "navigation@1"
//        },
//        {
//          "command": "ada83.check",
//          "when": "resourceLangId == ada83",
//          "group": "navigation@2"
//        }
//      ],
//      "commandPalette": [
//        {
//          "command": "ada83.build",
//          "when": "resourceLangId == ada83"
//        },
//        {
//          "command": "ada83.check",
//          "when": "resourceLangId == ada83"
//        }
//      ]
//    },
//    "chatInstructions": [
//      {
//        "path": "./ada83.instructions.md",
//        "when": "editorLangId == ada83"
//      }
//    ],
//    "languageModelTools": [
//      {
//        "name": "search_ada83_manual",
//        "toolReferenceName": "ada83Manual",
//        "displayName": "Search the Ada 83 Manual",
//        "canBeReferencedInPrompt": true,
//        "icon": "$(book)",
//        "tags": [
//          "ada",
//          "ada83"
//        ],
//        "userDescription": "Look a rule up in the bundled Ada 83 reference manual.",
//        "modelDescription": "Searches the full text of the Ada 83 Reference Manual (ANSI/MIL-STD-1815A, the 1983 standard) and returns the matching clauses verbatim, each headed by its clause number and title. Use this tool whenever a question of Ada 83 legality, semantics or wording comes up: whether a construct exists in Ada 83 at all, reserved words, syntax productions, visibility and scope, overload resolution, type and subtype rules, derived types, discriminants, generics, tasking, exceptions, representation clauses, attributes, pragmas, the predefined environment, or implementation permissions and dependencies. It is also the way to read a clause you already know by number. Prefer it to recalling Ada from memory, which is usually Ada 95, 2005, 2012 or 2022 and wrong here. Query it with a clause number such as 4.5.7 or 13.1, or with the Ada terms in question such as universal_integer, elaboration order, derived type, static expression or representation clause.",
//        "inputSchema": {
//          "type": "object",
//          "properties": {
//            "query": {
//              "type": "string",
//              "description": "A clause number such as 4.5.7, or the Ada 83 terms to look up, such as 'universal_integer' or 'overload resolution'."
//            }
//          },
//          "required": [
//            "query"
//          ]
//        }
//      }
//    ],
//    "problemMatchers": [
//      {
//        "name": "ada83",
//        "owner": "ada83",
//        "source": "ada83",
//        "fileLocation": [
//          "autoDetect",
//          "${workspaceFolder}"
//        ],
//        "pattern": {
//          "regexp": "^(\\S.*?):(\\d+):(\\d+):\\s+(error|warning|note|internal error):\\s+(.*)$",
//          "file": 1,
//          "line": 2,
//          "column": 3,
//          "severity": 4,
//          "message": 5
//        }
//      }
//    ],
//    "taskDefinitions": [
//      {
//        "type": "ada83",
//        "required": [
//          "task"
//        ],
//        "properties": {
//          "task": {
//            "type": "string",
//            "enum": [
//              "build",
//              "check"
//            ],
//            "description": "build compiles the active file to an executable; check resolves it and emits LLVM IR without writing anything."
//          }
//        }
//      }
//    ]
//  }
//}
//

//== README.md
//# Ada 83 for VS Code
//
//Ada 83 (ANSI/MIL-STD-1815A) support, answered by the `ada83` compiler
//itself. The extension starts `ada83 --lsp` and speaks the Language Server
//Protocol to it, so the squiggles, the definitions and the hovers come from
//the same lexer, parser and resolver that build the executable. The editor
//and the compiler cannot disagree about what Ada 83 is.
//
//## What it does
//
//- **Diagnostics as you type.** Errors, warnings and notes from the real
//  front end, with the notes that name a declaration turned into links.
//- **Go to definition**, into your own sources and into the runtime library.
//- **Hover**, showing the declaration as Ada, and where it was written.
//- **Highlight and find all references** for the name under the cursor.
//- **Outline and breadcrumbs** (`textDocument/documentSymbol`), and
//  **Go to Symbol in Workspace** (Ctrl+T) across the Ada sources in the
//  workspace root.
//- **Completion** from everything the compilation left visible, plus the 63
//  reserved words taken from the lexer's own table.
//- **Signature help** while a call is being written, showing the profile and
//  underlining the argument the cursor is on.
//- **Folding** of `is`/`begin`/`end`, records, cases, loops, ifs, selects
//  and runs of comment lines.
//- **Quick fixes** for a misspelt name: the compiler already works out what
//  you probably meant, and the extension offers it as an edit.
//- **Tasks and commands.** "Ada 83: Build This File" and "Ada 83: Check This
//  File" appear in the editor title bar and the command palette, and as
//  `ada83` tasks with the `$ada83` problem matcher.
//- **Syntax highlighting and snippets** for Ada 83 exactly: `tagged`,
//  `protected`, `aliased` and the rest of the later reserved words are
//  identifiers here, and are not painted as keywords.
//- **A manual search tool for chat.** `#ada83Manual` searches the full text
//  of the Ada 83 Reference Manual bundled beside the extension and returns
//  the matching clauses verbatim.
//
//## Installing
//
//The extension needs the compiler. Put `ada83` (and `ada83-runtime.ada`,
//which it looks for beside itself) somewhere on `PATH`, or point
//`ada83.compilerPath` at it.
//
//```
//code --install-extension ada83.vsix
//```
//
//## Settings
//
//| Setting | Default | Meaning |
//| --- | --- | --- |
//| `ada83.compilerPath` | `ada83` | The compiler to run as `ada83 --lsp`. |
//| `ada83.enable` | `true` | Report diagnostics as you type. |
//| `ada83.requestTimeout` | `15000` | Milliseconds to wait for one answer. |
//| `ada83.workspaceRequestTimeout` | `120000` | Milliseconds to wait for a workspace-wide symbol search. |
//
//## How it works
//
//Every answer is a compilation. The compiler is built to compile once and
//exit, so the language server never reuses its front end: each request runs
//a child process (`--analyse`, `--define`, `--describe`, `--symbols`,
//`--complete`, `--signature`) which compiles, prints JSON and exits, taking
//all of its state with it. Only the front end runs, so libLLVM is never
//loaded and the server starts instantly.
//
//## Licence
//
//See the compiler's `readme.md`.
//

//== ada83.instructions.md
//---
//name: Ada 83
//description: What Ada 83 (ANSI/MIL-STD-1815A) allows, and where to look it up.
//---
//
//These files are Ada 83 (ANSI/MIL-STD-1815A). They are not Ada 95, 2005, 2012
//or 2022, and the `ada83` compiler that checks them rejects anything later.
//
//Never write tagged types, protected objects or child units, and never write
//`abstract`, `aliased`, `requeue`, `until`, `interface`, `overriding`,
//`synchronized` or `some`: every one of those is Ada 95, 2005 or 2012. Ada 83
//has exactly 63 reserved words, and a construct needing a 64th is not Ada 83.
//
//Prefer the predefined packages the bundled runtime declares: `Standard`,
//`System`, `Text_IO` and its `Integer_IO`, `Float_IO`, `Fixed_IO` and
//`Enumeration_IO`, `Sequential_IO`, `Direct_IO`, `IO_Exceptions`, `Calendar`,
//`Low_Level_IO`, `Machine_Code`, `Unchecked_Conversion` and
//`Unchecked_Deallocation`.
//
//When legality is in question at all - visibility, overload resolution,
//representation clauses, implementation permissions, or whether a construct is
//Ada 83 in the first place - read the standard with the `search_ada83_manual`
//tool (`#ada83Manual`) instead of recalling modern Ada.
//

//== language-configuration.json
//{
//  "comments": { "lineComment": "--" },
//  "brackets": [["(", ")"]],
//  "autoClosingPairs": [
//    { "open": "(", "close": ")" },
//    { "open": "\"", "close": "\"", "notIn": ["string", "comment"] }
//  ],
//  "surroundingPairs": [["(", ")"], ["\"", "\""]],
//  "wordPattern": "[A-Za-z][A-Za-z0-9_]*",
//  "onEnterRules": [
//    {
//      "beforeText": "^\\s*--.*$",
//      "action": { "indent": "none", "appendText": "-- " }
//    }
//  ],
//  "indentationRules": {
//    "increaseIndentPattern": "(?i)^\\s*(.*\\b(is|then|loop|begin|record|declare|select|do|else)\\s*|.*\\b(procedure|function|package|task)\\b.*)$",
//    "decreaseIndentPattern": "(?i)^\\s*\\b(end|elsif|else|exception|when|begin|private)\\b"
//  }
//}
//

//== snippets.json
//{
//  "procedure body": {
//    "prefix": "procedure",
//    "body": ["procedure ${1:Name} is", "begin", "   $0", "end ${1:Name};"],
//    "description": "A procedure body"
//  },
//  "function body": {
//    "prefix": "function",
//    "body": ["function ${1:Name} return ${2:Type} is", "begin", "   $0",
//             "end ${1:Name};"],
//    "description": "A function body"
//  },
//  "package specification": {
//    "prefix": "package",
//    "body": ["package ${1:Name} is", "   $0", "end ${1:Name};"],
//    "description": "A package specification"
//  },
//  "package body": {
//    "prefix": "packagebody",
//    "body": ["package body ${1:Name} is", "   $0", "end ${1:Name};"],
//    "description": "A package body"
//  },
//  "for loop": {
//    "prefix": "for",
//    "body": ["for ${1:I} in ${2:1 .. 10} loop", "   $0", "end loop;"],
//    "description": "A for loop"
//  },
//  "while loop": {
//    "prefix": "while",
//    "body": ["while ${1:Condition} loop", "   $0", "end loop;"],
//    "description": "A while loop"
//  },
//  "if statement": {
//    "prefix": "if",
//    "body": ["if ${1:Condition} then", "   $0", "end if;"],
//    "description": "An if statement"
//  },
//  "case statement": {
//    "prefix": "case",
//    "body": ["case ${1:Expression} is", "   when ${2:Choice} =>", "      $0",
//             "   when others =>", "      null;", "end case;"],
//    "description": "A case statement"
//  },
//  "declare block": {
//    "prefix": "declare",
//    "body": ["declare", "   ${1:Name} : ${2:Type};", "begin", "   $0", "end;"],
//    "description": "A block statement with declarations"
//  },
//  "record type": {
//    "prefix": "record",
//    "body": ["type ${1:Name} is", "   record", "      ${2:Field} : ${3:Type};",
//             "   end record;"],
//    "description": "A record type declaration"
//  },
//  "exception handler": {
//    "prefix": "exception",
//    "body": ["exception", "   when ${1:Constraint_Error} =>", "      $0"],
//    "description": "An exception handler"
//  },
//  "hello world": {
//    "prefix": "hello",
//    "body": ["with Text_IO; use Text_IO;", "procedure ${1:Hello} is", "begin",
//             "   Put_Line (\"${2:Hello, Ada 83!}\");", "end ${1:Hello};"],
//    "description": "A complete program"
//  }
//}
//

//== syntaxes/ada83.tmLanguage.json
//{
//  "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
//  "name": "Ada 83",
//  "scopeName": "source.ada",
//  "patterns": [
//    {
//      "include": "#comment"
//    },
//    {
//      "include": "#string"
//    },
//    {
//      "include": "#character"
//    },
//    {
//      "include": "#number"
//    },
//    {
//      "include": "#declaration"
//    },
//    {
//      "include": "#attribute"
//    },
//    {
//      "include": "#predefined"
//    },
//    {
//      "include": "#literal"
//    },
//    {
//      "include": "#keyword"
//    },
//    {
//      "include": "#parameter"
//    },
//    {
//      "include": "#operator"
//    }
//  ],
//  "repository": {
//    "comment": {
//      "name": "comment.line.double-dash.ada",
//      "match": "--.*$"
//    },
//    "string": {
//      "name": "string.quoted.double.ada",
//      "begin": "\"",
//      "end": "\"",
//      "patterns": [
//        {
//          "name": "constant.character.escape.ada",
//          "match": "\"\""
//        }
//      ]
//    },
//    "character": {
//      "name": "string.quoted.single.ada",
//      "match": "'(?:[^']|'')'"
//    },
//    "number": {
//      "patterns": [
//        {
//          "name": "constant.numeric.based.ada",
//          "match": "\\b\\d+#[0-9A-Fa-f_]+#(?:[Ee][+-]?\\d+)?"
//        },
//        {
//          "name": "constant.numeric.decimal.ada",
//          "match": "\\b\\d[\\d_]*(?:\\.[\\d_]+)?(?:[Ee][+-]?\\d+)?\\b"
//        }
//      ]
//    },
//    "keyword": {
//      "patterns": [
//        {
//          "name": "keyword.control.ada",
//          "match": "(?i)\\b(abort|accept|access|all|array|at|begin|body|case|constant|declare|delay|delta|digits|do|else|elsif|end|entry|exception|exit|for|function|generic|goto|if|in|is|limited|loop|new|null|of|others|out|package|pragma|private|procedure|raise|range|record|renames|return|reverse|select|separate|subtype|task|terminate|then|type|use|when|while|with)\\b"
//        },
//        {
//          "name": "keyword.operator.word.ada",
//          "match": "(?i)\\b(abs|and|mod|not|or|rem|xor)\\b"
//        }
//      ]
//    },
//    "attribute": {
//      "name": "support.other.attribute.ada",
//      "match": "'[A-Za-z][A-Za-z0-9_]*"
//    },
//    "operator": {
//      "name": "keyword.operator.ada",
//      "match": "(:=|=>|\\.\\.|\\*\\*|/=|>=|<=|<<|>>|<>|[+\\-*/&<>=|])"
//    },
//    "declaration": {
//      "patterns": [
//        {
//          "match": "(?i)\\b(procedure|function|entry)\\s+([A-Za-z][A-Za-z0-9_]*)",
//          "captures": {
//            "1": {
//              "name": "keyword.control.ada"
//            },
//            "2": {
//              "name": "entity.name.function.ada"
//            }
//          }
//        },
//        {
//          "match": "(?i)\\b(package)\\s+(body\\s+)?([A-Za-z][A-Za-z0-9_]*)",
//          "captures": {
//            "1": {
//              "name": "keyword.control.ada"
//            },
//            "2": {
//              "name": "keyword.control.ada"
//            },
//            "3": {
//              "name": "entity.name.namespace.ada"
//            }
//          }
//        },
//        {
//          "match": "(?i)\\b(task)\\s+(body\\s+|type\\s+)?([A-Za-z][A-Za-z0-9_]*)",
//          "captures": {
//            "1": {
//              "name": "keyword.control.ada"
//            },
//            "2": {
//              "name": "keyword.control.ada"
//            },
//            "3": {
//              "name": "entity.name.class.ada"
//            }
//          }
//        },
//        {
//          "match": "(?i)\\b(type|subtype)\\s+([A-Za-z][A-Za-z0-9_]*)",
//          "captures": {
//            "1": {
//              "name": "keyword.control.ada"
//            },
//            "2": {
//              "name": "entity.name.type.ada"
//            }
//          }
//        },
//        {
//          "match": "(?i)\\b(generic|pragma)\\s+([A-Za-z][A-Za-z0-9_]*)?",
//          "captures": {
//            "1": {
//              "name": "keyword.control.ada"
//            },
//            "2": {
//              "name": "entity.name.tag.ada"
//            }
//          }
//        }
//      ]
//    },
//    "predefined": {
//      "name": "support.type.ada",
//      "match": "(?i)\\b(Integer|Natural|Positive|Float|Boolean|Character|String|Duration|Short_Integer|Long_Integer|Short_Float|Long_Float|System|Standard)\\b"
//    },
//    "literal": {
//      "patterns": [
//        {
//          "name": "constant.language.boolean.ada",
//          "match": "(?i)\\b(True|False)\\b"
//        },
//        {
//          "name": "support.class.exception.ada",
//          "match": "(?i)\\b(Constraint_Error|Numeric_Error|Program_Error|Storage_Error|Tasking_Error)\\b"
//        }
//      ]
//    },
//    "parameter": {
//      "match": "(?i)\\b([A-Za-z][A-Za-z0-9_]*)\\s*(:)\\s*((?:in\\s+out|in|out)\\b)?",
//      "captures": {
//        "1": {
//          "name": "variable.parameter.ada"
//        },
//        "2": {
//          "name": "punctuation.separator.ada"
//        },
//        "3": {
//          "name": "keyword.control.ada"
//        }
//      }
//    }
//  }
//}
//

//== extension.vsixmanifest
//<?xml version="1.0" encoding="utf-8"?>
//<PackageManifest Version="2.0.0"
//    xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011"
//    xmlns:d="http://schemas.microsoft.com/developer/vsx-schema-design/2011">
//  <Metadata>
//    <Identity Language="en-US" Id="ada83" Version="1.0.0" Publisher="ada83" />
//    <DisplayName>Ada 83</DisplayName>
//    <Description xml:space="preserve">Ada 83 (ANSI/MIL-STD-1815A) support. Diagnostics, go to definition, hover, outline, completion, signature help, folding and quick fixes come from the ada83 compiler itself.</Description>
//    <Tags>ada,ada83,mil-std-1815a</Tags>
//    <Categories>Programming Languages,Linters</Categories>
//    <GalleryFlags>Public</GalleryFlags>
//    <Properties>
//      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="^1.106.0" />
//      <Property Id="Microsoft.VisualStudio.Code.ExtensionDependencies" Value="" />
//    </Properties>
//  </Metadata>
//  <Installation>
//    <InstallationTarget Id="Microsoft.VisualStudio.Code" />
//  </Installation>
//  <Dependencies />
//  <Assets>
//    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
//    <Asset Type="Microsoft.VisualStudio.Services.Content.Details" Path="extension/README.md" Addressable="true" />
//  </Assets>
//</PackageManifest>
//

//== [Content_Types].xml
//<?xml version="1.0" encoding="utf-8"?>
//<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
//  <Default Extension="json" ContentType="application/json" />
//  <Default Extension="js" ContentType="application/javascript" />
//  <Default Extension="ada" ContentType="text/plain" />
//  <Default Extension="md" ContentType="text/markdown" />
//  <Default Extension="vsixmanifest" ContentType="text/xml" />
//</Types>
//

//== end
