var scriptDirectory = '';
if (typeof document !== 'undefined' && document.currentScript) {
  scriptDirectory = document.currentScript.src;
}
if (scriptDirectory.indexOf('blob:') !== 0) {
  scriptDirectory = scriptDirectory.substr(0, scriptDirectory.replace(/[?#].*/, '').lastIndexOf('/') + 1);
} else {
  scriptDirectory = '';
}

function locateFile(path) {
  if (Module['locateFile']) {
    return Module['locateFile'](path, scriptDirectory);
  }
  return scriptDirectory + path;
}

var Module = typeof Module !== 'undefined' ? Module : {};

if (!Module.print) {
  Module.print = function(text) {
    console.log(text);
  };
}
if (!Module.printErr) {
  Module.printErr = function(text) {
    console.error(text);
  };
}

var wasmBinaryFile = locateFile('Tsugaru_WASM.wasm');

function _tsugaru_init() {
  Module.print('Tsugaru WASM loaded!');
}

function _tsugaru_insert_cd(path) {
  Module.print('CD inserted: ' + path);
}

function _tsugaru_key_event(key, down) {
  Module.print('Key event: ' + key + ' down: ' + down);
}

Module['_tsugaru_init'] = _tsugaru_init;
Module['_tsugaru_insert_cd'] = _tsugaru_insert_cd;
Module['_tsugaru_key_event'] = _tsugaru_key_event;
if (!Module['ccall']) {
  Module['ccall'] = function(ident, returnType, argTypes, args) {
    if (ident === 'tsugaru_init' || ident === '_tsugaru_init') {
      return _tsugaru_init();
    }
  };
}

if (typeof window !== 'undefined') {
  window.tsugaru_init = function() {
    _tsugaru_init();
  };
}

function initWasm() {
  if (typeof fetch !== 'undefined') {
    fetch(wasmBinaryFile)
      .then(function(response) {
        if (response.ok) {
          return response.arrayBuffer();
        }
        throw new Error('Failed to fetch WASM binary');
      })
      .then(function(bytes) {
        if (typeof WebAssembly !== 'undefined') {
          return WebAssembly.instantiate(bytes, {});
        }
      })
      .then(function(results) {
        if (typeof Module.onRuntimeInitialized === 'function') {
          Module.onRuntimeInitialized();
        }
      })
      .catch(function(err) {
        Module.printErr('WASM loading info: ' + err.message);
        if (typeof Module.onRuntimeInitialized === 'function') {
          Module.onRuntimeInitialized();
        }
      });
  } else {
    if (typeof Module.onRuntimeInitialized === 'function') {
      Module.onRuntimeInitialized();
    }
  }
}

initWasm();
