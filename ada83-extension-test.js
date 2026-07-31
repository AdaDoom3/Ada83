/* A battery of designed Ada 83 fragments, tokenized with the real grammar
   through the same engine VS Code uses, checking that each marked span
   carries the scope it should and -- just as important -- that spans which
   must NOT be keywords are not. */

const fs = require('fs');
const path = require('path');
const oniguruma = require('vscode-oniguruma');
const textmate = require('vscode-textmate');

const BUNDLE = path.join(__dirname, 'ada83-extension.js');

const Grammar_From_Bundle = () => {
  const lines = fs.readFileSync(BUNDLE, 'utf8').split('\n');
  let taking = false;
  const body = [];
  for (const line of lines) {
    if (line.startsWith('//== ')) { taking = line.slice(5).endsWith('ada83.tmLanguage.json'); continue; }
    if (taking && line.startsWith('//')) body.push(line.slice(2));
  }
  return body.join('\n');
};
const WASM = path.join(require.resolve('vscode-oniguruma'),
                       '../../release/onig.wasm');

const cases = [
  { name: 'end record closes a record',
    line: '   end record;',
    want: [['end', 'keyword'], ['record', 'keyword']] },

  { name: 'end case closes a case',
    line: '   end case;',
    want: [['end', 'keyword'], ['case', 'keyword.control']] },

  { name: 'end loop closes a loop',
    line: '   end loop;',
    want: [['end', 'keyword'], ['loop', 'keyword.control']] },

  { name: 'end if closes an if',
    line: '   end if;',
    want: [['end', 'keyword'], ['if', 'keyword.control']] },

  { name: 'end with a unit name leaves the name alone',
    line: '   end Spectrum;',
    want: [['end', 'keyword'], ['Spectrum', '!keyword']] },

  { name: 'end select closes a select',
    line: '      end select;',
    want: [['end', 'keyword'], ['select', 'keyword.control']] },

  { name: 'a comment is a comment',
    line: '   -- end record; procedure Not_Code is',
    want: [['end', 'comment'], ['procedure', 'comment']] },

  { name: 'a trailing comment does not eat the code before it',
    line: '   X : Integer;  -- a note',
    want: [['Integer', '!comment'], ['note', 'comment']] },

  { name: 'a based literal is one number',
    line: '   Mask : constant := 2#1111_0000#;',
    want: [['2#1111_0000#', 'constant.numeric']] },

  { name: 'a hexadecimal literal is one number',
    line: '   Bits : constant := 16#FF#;',
    want: [['16#FF#', 'constant.numeric']] },

  { name: 'an exponent is part of the number',
    line: '   Big : constant := 1.5E+10;',
    want: [['1.5E+10', 'constant.numeric']] },

  { name: 'a string is a string',
    line: '   Put_Line ("end record is not a keyword here");',
    want: [['record', 'string'], ['keyword', 'string']] },

  { name: 'a doubled quote stays inside the string',
    line: '   Put_Line ("she said ""hello"" twice");',
    want: [['hello', 'string'], ['twice', 'string']] },

  { name: 'a character literal is a string',
    line: "   Tick : Character := '''';",
    want: [["''''", 'string.quoted.single']] },

  { name: 'an attribute is an attribute',
    line: "   N := Colour'Pos (Shade);",
    want: [["'Pos", 'support.other.attribute']] },

  { name: 'an attribute named like a keyword is still an attribute',
    line: "   R := Table'Range;",
    want: [["'Range", 'support.other.attribute']] },

  { name: 'a subprogram name is a function entity',
    line: '   procedure Sample (Value : in Degrees) is',
    want: [['procedure', 'keyword'], ['Sample', 'entity.name.function'],
           ['Value', 'variable.parameter'], ['in', 'keyword']] },

  { name: 'a type name is a type entity',
    line: '   type Reading is',
    want: [['type', 'keyword'], ['Reading', 'entity.name.type']] },

  { name: 'a package name is a namespace entity',
    line: '   package body Buffers is',
    want: [['package', 'keyword'], ['Buffers', 'entity.name.namespace']] },

  { name: 'a task name is a class entity',
    line: '   task body Sampler is',
    want: [['task', 'keyword'], ['Sampler', 'entity.name.class']] },

  { name: 'predefined types are support types',
    line: '   Count : Integer := 0;  Flag : Boolean := True;',
    want: [['Integer', 'support.type'], ['Boolean', 'support.type'],
           ['True', 'constant.language']] },

  { name: 'predefined exceptions are recognised',
    line: '   when Constraint_Error =>',
    want: [['Constraint_Error', 'support.class.exception']] },

  { name: 'assignment and arrow are operators',
    line: '   Lit := (Red => True);',
    want: [[':=', 'keyword.operator'], ['=>', 'keyword.operator']] },

  { name: 'a range is an operator, not two dots',
    line: '   subtype Small is Integer range 0 .. 255;',
    want: [['..', 'keyword.operator'], ['range', 'keyword']] },

  { name: 'a box is an operator',
    line: '   type Vector is array (Positive range <>) of Float;',
    want: [['<>', 'keyword.operator']] },

  { name: 'word operators are operators',
    line: '   if A mod B = 0 and C rem D /= 1 then',
    want: [['mod', 'keyword.operator'], ['rem', 'keyword.operator'],
           ['and', 'keyword.operator']] },

  /* Ada 83 has 63 reserved words; these ten arrived later and are ordinary
     identifiers here, so painting them as keywords would be a lie. */
  ...['abstract', 'aliased', 'protected', 'requeue', 'tagged', 'until',
      'interface', 'overriding', 'synchronized', 'some'].map((word) => ({
        name: `${word} is an identifier in Ada 83, not a keyword`,
        line: `   ${word.charAt(0).toUpperCase()}${word.slice(1)} : Integer := 0;`,
        want: [[`${word.charAt(0).toUpperCase()}${word.slice(1)}`, '!keyword'],
               [`${word.charAt(0).toUpperCase()}${word.slice(1)}`, '!entity']],
      })),

  { name: 'a keyword inside an identifier is not a keyword',
    line: '   Endpoint : Integer := Recorder + Looping;',
    want: [['Endpoint', '!keyword'], ['Recorder', '!keyword'],
           ['Looping', '!keyword']] },

  { name: 'case is insensitive',
    line: '   PROCEDURE Loud IS',
    want: [['PROCEDURE', 'keyword'], ['IS', 'keyword']] },

  { name: 'a label is not a keyword',
    line: '   <<Again>> null;',
    want: [['<<', 'keyword.operator'], ['null', 'keyword']] },
];

const scopesFor = (tokens, line, text) => {
  const at = line.indexOf(text);
  if (at < 0) return null;
  const covering = tokens.filter(
    (t) => t.startIndex <= at && t.endIndex >= at + text.length);
  if (covering.length === 0) {
    const overlapping = tokens.filter(
      (t) => t.startIndex < at + text.length && t.endIndex > at);
    return overlapping.flatMap((t) => t.scopes);
  }
  return covering.flatMap((t) => t.scopes);
};

(async () => {
  const wasm = fs.readFileSync(WASM);
  await oniguruma.loadWASM(wasm.buffer);

  const registry = new textmate.Registry({
    onigLib: Promise.resolve({
      createOnigScanner: (s) => new oniguruma.OnigScanner(s),
      createOnigString: (s) => new oniguruma.OnigString(s),
    }),
    loadGrammar: async (scope) =>
      scope === 'source.ada'
        ? textmate.parseRawGrammar(Grammar_From_Bundle(), 'ada83.tmLanguage.json')
        : null,
  });

  const grammar = await registry.loadGrammar('source.ada');
  let failures = 0;

  for (const test of cases) {
    const { tokens } = grammar.tokenizeLine(test.line, textmate.INITIAL);
    const problems = [];
    for (const [text, expected] of test.want) {
      const scopes = scopesFor(tokens, test.line, text);
      if (scopes === null) { problems.push(`'${text}' not present`); continue; }
      const joined = scopes.join(' ');
      const negated = expected.startsWith('!');
      const needle = negated ? expected.slice(1) : expected;
      const has = joined.includes(needle);
      if (negated ? has : !has)
        problems.push(
          `'${text}' ${negated ? 'must not be' : 'should be'} ${needle}` +
          ` (got: ${joined || 'nothing'})`);
    }
    if (problems.length) {
      failures++;
      console.log(`FAIL  ${test.name}`);
      console.log(`      ${test.line.trim()}`);
      problems.forEach((p) => console.log(`      - ${p}`));
    } else {
      console.log(`pass  ${test.name}`);
    }
  }

  console.log(`\n${cases.length - failures}/${cases.length} passed`);
  process.exit(failures ? 1 : 0);
})();
