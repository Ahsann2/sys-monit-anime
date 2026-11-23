let nativePort = null;
let pollTimer = null;

function connectNativeHost() {
  if (nativePort) return;
  nativePort = browser.runtime.connectNative("com.vivoguard.browser");
  nativePort.onMessage.addListener((message) => {
    if (message && message.command === "close_tabs") {
      browser.tabs.query({}).then((tabs) => {
        const ids = tabs
          .filter((tab) => !tab.pinned)
          .map((tab) => tab.id)
          .filter((id) => Number.isInteger(id));
        if (ids.length > 0) return browser.tabs.remove(ids);
        return undefined;
      });
    }
  });
  nativePort.onDisconnect.addListener(() => {
    nativePort = null;
    if (pollTimer) clearInterval(pollTimer);
    setTimeout(connectNativeHost, 3000);
  });
  pollTimer = setInterval(() => {
    if (nativePort) {
      nativePort.postMessage({ action: "poll", browser: "firefox" });
    }
  }, 1000);
}

connectNativeHost();
