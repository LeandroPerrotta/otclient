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

const luaCallbacks = {};

window.registerLuaCallback = function (name, cb) {
  luaCallbacks[name] = cb;
};

window.receiveFromLua = function (message) {
  const handler = luaCallbacks[message.name];
  if (handler) {
    handler(message.data);
  }
};

sendToLua('js_loaded');
