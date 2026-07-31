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
      { scheme: 'file', language: 'ada83' }, Definition_Provider));
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
//    "vscode": "^1.75.0"
//  },
//  "categories": [
//    "Programming Languages",
//    "Linters"
//  ],
//  "activationEvents": [
//    "onLanguage:ada83"
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
//        }
//      }
//    },
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
//        "type": "ada83"
//      }
//    ]
//  }
//}
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
//    <Description xml:space="preserve">Ada 83 (ANSI/MIL-STD-1815A) support. Diagnostics, go to definition and highlighting come from the ada83 compiler itself.</Description>
//    <Tags>ada,ada83,mil-std-1815a</Tags>
//    <Categories>Programming Languages,Linters</Categories>
//    <GalleryFlags>Public</GalleryFlags>
//    <Properties>
//      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="^1.75.0" />
//      <Property Id="Microsoft.VisualStudio.Code.ExtensionDependencies" Value="" />
//    </Properties>
//  </Metadata>
//  <Installation>
//    <InstallationTarget Id="Microsoft.VisualStudio.Code" />
//  </Installation>
//  <Dependencies />
//  <Assets>
//    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true" />
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
