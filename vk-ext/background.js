const SERVER = "http://127.0.0.1:8002/update";
const VK_URLS = ["https://vk.ru", "https://vk.com", "https://login.vk.ru"];
let lastSent = 0;

async function sendCookies() {
  try {
    const cookies = [];
    for (const url of VK_URLS) {
      cookies.push(...(await browser.cookies.getAll({ url })));
    }
    if (cookies.length === 0) return false;

    const payload = cookies.map((c) => ({
      name: c.name,
      value: c.value,
      host: c.domain,
    }));
    const resp = await fetch(SERVER, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
    });
    return resp.ok;
  } catch (e) {
    console.warn("VK cookies: приёмник недоступен", e.message);
    return false;
  }
}

function isVkCookie(changeInfo) {
  const host = (changeInfo.cookie && changeInfo.cookie.domain) || "";
  return host.endsWith("vk.ru") || host.endsWith("vk.com");
}

browser.cookies.onChanged.addListener((changeInfo) => {
  if (!isVkCookie(changeInfo)) return;
  const now = Date.now();
  if (now - lastSent < 2000) return;
  lastSent = now;
  setTimeout(sendCookies, 500);
});

browser.runtime.onMessage.addListener(async (msg) => {
  if (msg === "send") {
    const ok = await sendCookies();
    return { ok: ok, error: ok ? null : "приёмник недоступен" };
  }
});