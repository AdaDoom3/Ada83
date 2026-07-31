'use strict';

const { spawn } = require('child_process');
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

const Initialize = (Process_Id) => ({
  id: 1,
  method: 'initialize',
  params: { processId: Process_Id, rootUri: null, capabilities: {} },
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

const Diagnostic_Of = (Reported) =>
  Object.assign (
    new vscode.Diagnostic (Range_Of (Reported.range), Reported.message,
                           Severity_Of (Reported.severity)),
    { source: 'ada83' });

const Location_Of = (Result) =>
  Result === null || Result === undefined
    ? null
    : new vscode.Location (vscode.Uri.parse (Result.uri),
                           Range_Of (Result.range));

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
const Awaiting = new Map ();

const Send = (Message) => {
  if (Session !== null && Session.Server.stdin.writable)
    Session.Server.stdin.write (Framed (Message));
};

const Settle = ({ id, result }) =>
  ((Resolve) => {
     if (Resolve === undefined) return;
     Awaiting.delete (id);
     Resolve (result ?? null);
   }) (Awaiting.get (id));

const Receive = (Chunk) => {
  if (Session === null) return;
  const { Messages, Remainder } = Unframed (Session.Buffered + Chunk);
  Session = { ...Session, Buffered: Remainder };
  const Parsed_Messages = Messages.map (Parsed);
  Publications_In (Parsed_Messages).forEach (({ Uri, Diagnostics }) =>
    Session.Diagnostics.set (Uri, Diagnostics));
  Answers_In (Parsed_Messages).forEach (Settle);
};

const Ask = (Build) =>
  ((Id) =>
     new Promise ((Resolve) => {
       Awaiting.set (Id, Resolve);
       Send (Build (Id));
       setTimeout (() => {
         if (Awaiting.delete (Id)) Resolve (null);
       }, 5000);
     })) (Next_Id++);

const Definition_Provider = {
  provideDefinition: (Document, Position) =>
    Session === null
      ? null
      : Ask ((Id) => Definition (Id, Document, Position)).then (Location_Of),
};

const Highlight_Provider = {
  provideDocumentHighlights: (Document, Position) =>
    Session === null
      ? []
      : Ask ((Id) => Document_Highlight (Id, Document, Position))
          .then (Highlights_Of),
};

const Reference_Provider = {
  provideReferences: (Document, Position, Context) =>
    Session === null
      ? []
      : Ask ((Id) => References (Id, Document, Position,
                                 (Context ?? {}).includeDeclaration !== false))
          .then (Locations_Of),
};

const Report_Failure = (Command) => (Reason) =>
  vscode.window.showErrorMessage (
    `Ada 83: cannot run '${Command} --lsp' (${Reason.message}). ` +
    'Set ada83.compilerPath to the compiler, and keep ada83-runtime.ada ' +
    'beside it.');

const Start = (Diagnostics, Output) => {
  const Settings = vscode.workspace.getConfiguration ('ada83');
  if (!Settings.get ('enable', true)) return;

  const Command = Settings.get ('compilerPath', 'ada83');
  const Server = spawn (Command, ['--lsp'], { stdio: 'pipe' });

  Session = { Server, Diagnostics, Buffered: '' };

  Server.on ('error', Report_Failure (Command));
  Server.stdout.setEncoding ('utf8');
  Server.stdout.on ('data', Receive);
  Server.stderr.setEncoding ('utf8');
  Server.stderr.on ('data', (Text) => Output.append (Text));

  [Initialize (process.pid), Initialized ()].forEach (Send);
  vscode.workspace.textDocuments
    .filter (Is_Ada_Document)
    .map (Did_Open)
    .forEach (Send);
};

const Stop = () => {
  if (Session === null) return;
  [Shutdown (), Exit ()].forEach (Send);
  Session = null;
};

const On_Ada = (Notify) => (Event) =>
  ((Document) => { if (Is_Ada_Document (Document)) Send (Notify (Document)); })
    (Event.document ?? Event);

const activate = (Context) => {
  const Output = vscode.window.createOutputChannel ('Ada 83');
  const Diagnostics = vscode.languages.createDiagnosticCollection ('ada83');

  Start (Diagnostics, Output);

  Context.subscriptions.push (
    Output,
    Diagnostics,
    { dispose: Stop },
    vscode.workspace.onDidOpenTextDocument (On_Ada (Did_Open)),
    vscode.workspace.onDidChangeTextDocument (On_Ada (Did_Change)),
    vscode.workspace.onDidCloseTextDocument (On_Ada (Did_Close)),
    vscode.languages.registerDefinitionProvider (
      { scheme: 'file', language: 'ada83' }, Definition_Provider),
    vscode.languages.registerDocumentHighlightProvider (
      { scheme: 'file', language: 'ada83' }, Highlight_Provider),
    vscode.languages.registerReferenceProvider (
      { scheme: 'file', language: 'ada83' }, Reference_Provider));
};

const deactivate = Stop;

module.exports = { activate, deactivate };
