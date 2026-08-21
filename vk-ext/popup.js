const statusEl = document.getElementById("status");
const btn = document.getElementById("send");

btn.addEventListener("click", async () => {
  statusEl.textContent = "отправляю...";
  const res = await browser.runtime.sendMessage("send");
  if (res && res.ok) {
    statusEl.textContent = "куки обновлены";
  } else {
    statusEl.textContent =
      "ошибка: " + ((res && res.error) || "неизвестно") +
      " — приёмник запущен? (systemctl status vk-cookie-server)";
  }
});