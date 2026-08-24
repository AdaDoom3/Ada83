" ada83.vim -- Ada 83 support for Vim and Neovim: syntax highlighting, the
" ada83 language server, and semantic highlighting driven by the compiler.
"
" Install by dropping this one file into a plugin directory:
"
"     ~/.vim/plugin/ada83.vim                       (Vim)
"     ~/.config/nvim/plugin/ada83.vim               (Neovim)
"
" or by sourcing it from your vimrc:  source /path/to/ada83.vim
"
" What it gives you:
"
"   * The 'ada83' filetype for *.ada, *.ads, *.adb and *.a, with a syntax
"     that needs nothing but Vim -- the reserved words, the literals, the
"     attributes, the pragmas, the labels and the declared names.
"   * Semantic highlighting from the compiler itself.  `ada83 --highlight`
"     prints every token of a file with the kind the analysis resolved it
"     to, so a name is coloured as the package, type, function, variable,
"     parameter or enumeration literal it was declared as, rather than as
"     whatever a regular expression guessed.  Applied over the syntax as
"     text properties (Vim) or extmarks (Neovim) when the file is read and
"     after it is written, and on demand with :Ada83Highlight.  On Neovim
"     the run is done in Lua and asynchronously, so it costs the typist
"     nothing even on a unit of thousands of lines.
"   * The language server, `ada83 --lsp`, started for the buffer: Neovim's
"     built-in client, or vim-lsp if it is installed.  For coc.nvim, put
"
"         "languageserver": {
"           "ada83": { "command": "ada83", "args": ["--lsp"],
"                      "filetypes": ["ada83"] }
"         }
"
"     in your coc-settings.json instead.
"
" Settings, all optional:
"
"     let g:ada83_command = 'ada83'   " compiler, found on $PATH by default
"     let g:ada83_lsp = 1             " start the language server
"     let g:ada83_semantic_highlight = 1
"     let g:ada83_highlight_limit = 20000   " tokens applied per buffer
"     let g:ada83_filetype = 1        " claim *.ada, *.ads, *.adb and *.a
"                                     " from Vim's own 'ada' filetype

if exists('g:loaded_ada83')
  finish
endif
let g:loaded_ada83 = 1

let s:save_cpo = &cpo
set cpo&vim

if !exists('g:ada83_command')
  let g:ada83_command = 'ada83'
endif
if !exists('g:ada83_lsp')
  let g:ada83_lsp = 1
endif
if !exists('g:ada83_semantic_highlight')
  let g:ada83_semantic_highlight = 1
endif
if !exists('g:ada83_highlight_limit')
  let g:ada83_highlight_limit = 20000
endif
if !exists('g:ada83_filetype')
  let g:ada83_filetype = 1
endif

" The syntax groups, and the semantic kinds --highlight names, share one
" table: the same name means the same colour whichever way it was decided.
let s:groups = {
      \ 'comment':     'Comment',
      \ 'string':      'String',
      \ 'character':   'Character',
      \ 'number':      'Number',
      \ 'keyword':     'Keyword',
      \ 'attribute':   'Special',
      \ 'pragma':      'PreProc',
      \ 'label':       'Label',
      \ 'operator':    'Operator',
      \ 'delimiter':   'Delimiter',
      \ 'namespace':   'Structure',
      \ 'type':        'Type',
      \ 'function':    'Function',
      \ 'variable':    'Identifier',
      \ 'parameter':   'Identifier',
      \ 'enumeration': 'Constant',
      \ 'exception':   'Exception',
      \ 'boolean':     'Boolean',
      \ 'null':        'Constant',
      \ 'todo':        'Todo',
      \ }

" Kinds --highlight reports that no property is worth spending: an
" unresolved name should keep whatever the syntax made of it.
let s:unpropertied = ['identifier']

function! s:Executable() abort
  return executable(g:ada83_command) ? g:ada83_command : ''
endfunction

" The directory the compiler searches for the units this one withs: the
" nearest ancestor holding a project file, else the file's own directory.
function! s:RootDirectory(path) abort
  let l:directory = fnamemodify(a:path, ':p:h')
  let l:scan = l:directory
  while 1
    if !empty(glob(l:scan . '/*.gpr', 1)) || !empty(glob(l:scan . '/*.gpj', 1))
      return l:scan
    endif
    let l:parent = fnamemodify(l:scan, ':h')
    if l:parent ==# l:scan
      return l:directory
    endif
    let l:scan = l:parent
  endwhile
endfunction

" ---------------------------------------------------------------- syntax

function! s:Syntax() abort
  if exists('b:current_syntax')
    return
  endif
  syntax clear
  syntax case ignore
  syntax sync minlines=200

  syntax keyword ada83Keyword abort abs accept access all and array at begin
  syntax keyword ada83Keyword body case constant declare delay delta digits do
  syntax keyword ada83Keyword else elsif end entry exception exit for function
  syntax keyword ada83Keyword generic goto if in is limited loop mod new not of
  syntax keyword ada83Keyword or others out package pragma private procedure
  syntax keyword ada83Keyword raise range record rem renames return reverse
  syntax keyword ada83Keyword select separate subtype task terminate then type
  syntax keyword ada83Keyword use when while with xor

  syntax keyword ada83Null    null
  syntax keyword ada83Boolean true false

  syntax keyword ada83Type Integer Natural Positive Float Boolean Character
  syntax keyword ada83Type String Duration Short_Integer Long_Integer
  syntax keyword ada83Type Short_Float Long_Float Short_Short_Integer
  syntax keyword ada83Type Long_Long_Integer Universal_Integer Universal_Real
  syntax keyword ada83Type File_Type File_Mode Address Priority

  syntax keyword ada83Namespace Standard ASCII System Calendar Text_IO
  syntax keyword ada83Namespace Sequential_IO Direct_IO IO_Exceptions
  syntax keyword ada83Namespace Low_Level_IO Machine_Code Integer_IO Float_IO
  syntax keyword ada83Namespace Fixed_IO Enumeration_IO Unchecked_Conversion
  syntax keyword ada83Namespace Unchecked_Deallocation

  syntax keyword ada83Exception Constraint_Error Numeric_Error Program_Error
  syntax keyword ada83Exception Storage_Error Tasking_Error Status_Error
  syntax keyword ada83Exception Mode_Error Name_Error Use_Error Device_Error
  syntax keyword ada83Exception End_Error Data_Error Layout_Error Time_Error

  " Where two matches begin at the same character the later definition
  " wins, so the general items are stated first and everything more
  " specific after them.
  syntax match ada83Delimiter "[();,.:]"
  syntax match ada83Operator  ":=\|=>\|\.\.\|\*\*\|/=\|>=\|<=\|<>\|[-+*/&<>=|]"

  syntax match ada83Number
        \ "\<\d[0-9_]*#[0-9A-Fa-f_]\+\%(\.[0-9A-Fa-f_]\+\)\=#\%([Ee][+-]\=\d[0-9_]*\)\="
  syntax match ada83Number
        \ "\<\d[0-9_]*\%(\.[0-9_]\+\)\=\%([Ee][+-]\=\d[0-9_]*\)\="

  " A declared name takes the colour of what declares it, so a unit reads
  " the same before the compiler has been asked and after.  The name is
  " reached by looking behind rather than with \zs: a keyword outranks any
  " match that starts where it starts, so a match beginning at 'procedure'
  " would be thrown away before its \zs was ever consulted.
  syntax match ada83Function
        \ "\%(\<\%(procedure\|function\|entry\|accept\)\s\+\)\@40<=\h\w*"
  syntax match ada83Type
        \ "\%(\<\%(type\|subtype\)\s\+\)\@40<=\h\w*"
  syntax match ada83Type
        \ "\%(\<task\s\+\%(body\s\+\|type\s\+\)\=\)\@40<=\h\w*"
  syntax match ada83Namespace
        \ "\%(\<package\s\+\%(body\s\+\)\=\)\@40<=\h\w*\%(\.\h\w*\)*"
  syntax match ada83PragmaName "\%(\<pragma\s\+\)\@40<=\h\w*"

  " A tick opens a character literal only where no value can stand before
  " it; after a name or a ')' it marks an attribute instead, so T'First and
  " Character'('A') both read the way the compiler reads them.
  syntax match ada83Attribute "\%(\k\|)\)\@<='\h\w*"ms=s+1
  syntax match ada83Character "\%(\k\|)\)\@<!'.'"

  " A name before a colon that is not ':=' is being declared -- an object,
  " a component or a formal parameter -- and the name between 'for' and
  " 'in' is a loop parameter.
  syntax match ada83Variable "\h\w*\ze\s*:\%(=\)\@!"
  syntax match ada83Variable "\%(\<for\s\+\)\@10<=\h\w*\%(\s\+in\>\)\@="

  syntax match ada83Label "<<\s*\h\w*\s*>>"
  syntax match ada83Label
        \ "^\s*\zs\h\w*\ze\s*:\s*\%(for\>\|while\>\|loop\>\|declare\>\|begin\>\|$\)"

  syntax region ada83String  start=+"+ skip=+""+ end=+"+ oneline
        \ contains=@Spell
  " An operator designator is a string in every other position, so its
  " declaration has to be stated after the region that would swallow it.
  syntax match ada83Function
        \ "\%(\<\%(procedure\|function\)\s\+\)\@40<=\"[^\"]\+\""

  syntax match ada83Comment  "--.*$" contains=ada83Todo,@Spell
  syntax keyword ada83Todo contained TODO FIXME XXX NOTE

  for [l:kind, l:target] in items(s:groups)
    execute 'highlight default link ada83' . toupper(l:kind[0]) . l:kind[1:]
          \ . ' ' . l:target
  endfor
  highlight default link ada83PragmaName ada83Pragma

  let b:current_syntax = 'ada83'
endfunction

" ------------------------------------------------- semantic highlighting

let s:namespace = has('nvim') ? nvim_create_namespace('ada83') : 0
let s:property_types_added = 0

" On Neovim the whole run -- the compiler, the decode and the marking -- is
" done in Lua, which is both several times quicker than the loop below and
" asynchronous, so a large unit costs nothing the typist can feel.  Vim
" keeps the Vimscript path.
let s:lua = has('nvim') && luaeval('vim.system ~= nil')

if s:lua
lua << ADA83_LUA
local ada83     = {}
local namespace = vim.api.nvim_create_namespace('ada83')

-- One token is [line, column, length, kind], counted from one, with the
-- column and the length in bytes -- which is what an extmark wants, so
-- nothing has to be re-measured here.  The kind is an index into the
-- legend the compiler prints beside them.
function ada83.highlight(buffer, command, path, root, limit, spoken)
  vim.system({ command, '--highlight', path, root }, { text = true },
    function(result)
      if result.code ~= 0 or result.stdout == '' then
        return
      end
      local ok, answer = pcall(vim.json.decode, result.stdout)
      if not ok or type(answer) ~= 'table' or not answer.tokens then
        return
      end

      local groups = {}
      for index, kind in ipairs(answer.legend or {}) do
        groups[index - 1] = kind ~= 'identifier'
          and ('ada83' .. kind:sub(1, 1):upper() .. kind:sub(2))
          or false
      end

      vim.schedule(function()
        if not vim.api.nvim_buf_is_valid(buffer) then
          return
        end
        vim.api.nvim_buf_clear_namespace(buffer, namespace, 0, -1)

        local set     = vim.api.nvim_buf_set_extmark
        local lines   = vim.api.nvim_buf_line_count(buffer)
        local applied = 0
        for _, token in ipairs(answer.tokens) do
          local group = groups[token[4]]
          if not group or applied >= limit then
            goto continue
          end
          if token[1] <= lines then
            set(buffer, namespace, token[1] - 1, token[2] - 1, {
              end_col  = token[2] - 1 + token[3],
              hl_group = group,
              priority = 110,
              strict   = false,
            })
            applied = applied + 1
          end
          ::continue::
        end

        vim.b[buffer].ada83_highlight_count = applied
        if spoken then
          vim.notify('ada83: coloured ' .. applied .. ' tokens')
        end
      end)
    end)
end

_G.ada83 = ada83
ADA83_LUA
endif

function! s:PropertyName(kind) abort
  return 'ada83_' . a:kind
endfunction

function! s:AddPropertyTypes() abort
  if s:property_types_added
    return
  endif
  for [l:kind, l:target] in items(s:groups)
    if index(s:unpropertied, l:kind) >= 0
      continue
    endif
    let l:name = s:PropertyName(l:kind)
    if empty(prop_type_get(l:name))
      call prop_type_add(l:name, {'highlight': l:target, 'priority': 10})
    endif
  endfor
  let s:property_types_added = 1
endfunction

function! s:ClearHighlight(buffer) abort
  if has('nvim')
    call nvim_buf_clear_namespace(a:buffer, s:namespace, 0, -1)
  elseif has('textprop')
    for l:kind in keys(s:groups)
      if index(s:unpropertied, l:kind) < 0 &&
            \ !empty(prop_type_get(s:PropertyName(l:kind)))
        call prop_remove({'type': s:PropertyName(l:kind), 'bufnr': a:buffer,
              \ 'all': 1})
      endif
    endfor
  endif
endfunction

" One token is [line, column, length, kind], counted from one, with the
" column and the length in bytes -- which is what both text properties and
" extmarks want, so nothing has to be re-measured here.
function! s:ApplyToken(buffer, legend, token) abort
  let l:kind = get(a:legend, a:token[3], '')
  if empty(l:kind) || index(s:unpropertied, l:kind) >= 0
        \ || !has_key(s:groups, l:kind)
    return 0
  endif
  try
    if has('nvim')
      call nvim_buf_set_extmark(a:buffer, s:namespace, a:token[0] - 1,
            \ a:token[1] - 1,
            \ {'end_col': a:token[1] - 1 + a:token[2],
            \  'hl_group': 'ada83' . toupper(l:kind[0]) . l:kind[1:],
            \  'priority': 110})
    else
      call prop_add(a:token[0], a:token[1], {'length': a:token[2],
            \ 'bufnr': a:buffer, 'type': s:PropertyName(l:kind)})
    endif
  catch
    return 0
  endtry
  return 1
endfunction

function! Ada83SemanticHighlight(...) abort
  let l:buffer = bufnr('%')
  if &filetype !=# 'ada83'
    return
  endif
  if !has('nvim') && !has('textprop')
    if a:0 | echomsg 'ada83: this Vim has no text properties' | endif
    return
  endif

  let l:compiler = s:Executable()
  if empty(l:compiler)
    if a:0 | echomsg 'ada83: ' . g:ada83_command . ' is not on $PATH' | endif
    return
  endif

  " The compiler reads the file from disk, so an unwritten buffer would be
  " coloured from a stale text; leave the syntax alone until it is saved.
  let l:path = expand('%:p')
  if empty(l:path) || !filereadable(l:path) || &modified
    if a:0 | echomsg 'ada83: save the buffer first' | endif
    return
  endif

  let l:root = s:RootDirectory(l:path)

  if s:lua
    call luaeval('ada83.highlight(_A.buffer, _A.command, _A.path, _A.root,'
          \ . ' _A.limit, _A.spoken)',
          \ {'buffer': l:buffer, 'command': l:compiler, 'path': l:path,
          \  'root': l:root, 'limit': g:ada83_highlight_limit,
          \  'spoken': a:0 ? v:true : v:false})
    return
  endif

  call s:AddPropertyTypes()

  let l:answer = system(printf('%s --highlight %s %s 2>%s',
        \ shellescape(l:compiler), shellescape(l:path), shellescape(l:root),
        \ has('win32') ? 'NUL' : '/dev/null'))
  if v:shell_error != 0 || empty(l:answer)
    if a:0 | echomsg 'ada83: --highlight reported nothing' | endif
    return
  endif

  try
    let l:parsed = json_decode(l:answer)
  catch
    if a:0 | echomsg 'ada83: --highlight did not answer JSON' | endif
    return
  endtry
  if type(l:parsed) != v:t_dict || !has_key(l:parsed, 'tokens')
    return
  endif

  call s:ClearHighlight(l:buffer)
  let l:legend = get(l:parsed, 'legend', [])
  let l:lines = line('$')
  let l:applied = 0
  for l:token in l:parsed.tokens
    if l:applied >= g:ada83_highlight_limit
      break
    endif
    if l:token[0] <= l:lines
      let l:applied += s:ApplyToken(l:buffer, l:legend, l:token)
    endif
  endfor
  let b:ada83_highlight_count = l:applied
endfunction

command! Ada83Highlight      call Ada83SemanticHighlight(1)
command! Ada83HighlightClear call s:ClearHighlight(bufnr('%'))

" ------------------------------------------------------- language server

function! s:StartLanguageServer() abort
  if !g:ada83_lsp || exists('b:ada83_lsp_started')
    return
  endif
  let l:compiler = s:Executable()
  if empty(l:compiler)
    return
  endif
  let l:path = expand('%:p')
  if empty(l:path)
    return
  endif
  let b:ada83_lsp_started = 1

  if has('nvim-0.8')
    call luaeval('vim.lsp.start({ name = "ada83", cmd = _A.cmd,'
          \ . ' root_dir = _A.root })',
          \ {'cmd': [l:compiler, '--lsp'], 'root': s:RootDirectory(l:path)})
  elseif exists('*lsp#register_server')
    if empty(lsp#get_server_info('ada83'))
      call lsp#register_server({
            \ 'name': 'ada83',
            \ 'cmd': {server_info -> [l:compiler, '--lsp']},
            \ 'allowlist': ['ada83'],
            \ 'whitelist': ['ada83'],
            \ })
    endif
  endif
endfunction

" -------------------------------------------------------------- filetype

function! s:Setup() abort
  setlocal comments=:--
  setlocal commentstring=--\ %s
  setlocal formatoptions-=t formatoptions+=croql
  setlocal suffixesadd=.ada,.ads,.adb

  call s:Syntax()
  call s:StartLanguageServer()
  if g:ada83_semantic_highlight
    call Ada83SemanticHighlight()
  endif
endfunction

augroup ada83
  autocmd!
  " Vim ships an 'ada' filetype of its own and claims these suffixes first,
  " so the filetype is set rather than merely defaulted: our autocommand is
  " registered later and this is what makes it win.
  autocmd BufRead,BufNewFile *.ada,*.ads,*.adb,*.a
        \ if g:ada83_filetype | setlocal filetype=ada83 | endif
  autocmd FileType ada83 call s:Setup()
  autocmd Syntax   ada83 call s:Syntax()
  autocmd BufWritePost *.ada,*.ads,*.adb,*.a
        \ if &filetype ==# 'ada83' && g:ada83_semantic_highlight |
        \   call Ada83SemanticHighlight() |
        \ endif
augroup END

let &cpo = s:save_cpo
unlet s:save_cpo
