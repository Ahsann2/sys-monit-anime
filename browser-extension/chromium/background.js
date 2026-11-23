let nativePort = null;
let pollTimer = null;

function closeAllTabs() {
  chrome.tabs.query({}, (tabs) => {
    const ids = tabs
      .filter((tab) => !tab.pinned)
      .map((tab) => tab.id)
      .filter((id) => Number.isInteger(id));
    if (ids.length > 0) {
      chrome.tabs.remove(ids);
    }
  });
}

function connectNativeHost() {
  if (nativePort) return;
  try {
    nativePort = chrome.runtime.connectNative("com.vivoguard.browser");
    nativePort.onMessage.addListener((message) => {
      if (message && message.command === "close_tabs") {
        closeAllTabs();
      }
    });
    nativePort.onDisconnect.addListener(() => {
      nativePort = null;
      if (pollTimer) clearInterval(pollTimer);
      pollTimer = setTimeout(connectNativeHost, 3000);
    });
    pollTimer = setInterval(() => {
      if (nativePort) {
        nativePort.postMessage({ action: "poll", browser: "chromium" });
      }
    }, 1000);
  } catch (_) {
    nativePort = null;
    pollTimer = setTimeout(connectNativeHost, 5000);
  }
}

connectNativeHost();
