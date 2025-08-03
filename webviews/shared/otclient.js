function sendToLua(name, data) {
  if (typeof luaCallback !== 'function') {
    return;
  }

  luaCallback({
    request: JSON.stringify({ name, data }),
    onSuccess: () => {},
    onFailure: (code, msg) => console.error(msg),
  });
}

sendToLua('js_loaded');
