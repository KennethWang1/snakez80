/** @type { HTMLCanvasElement } */
const canvas = document.getElementById("canvas");
canvas.width = canvas.height = 16;
const ctx = canvas.getContext("2d", { alpha: false });

const msgs = document.getElementById("msgs");
const logs = document.getElementById("logs");

let textLogs = "";

let frameByteIndex = null;

const imageData = ctx.createImageData(16, 16);

const COLORS = {
  [0]: [0, 0, 0],
  [1]: [200, 200, 200],
  [2]: [0, 128, 0],
  [3]: [128, 0, 0],

  invalid: [255, 255, 0],
};

async function readPump(port) {
  while (port.readable) {
    const reader = port.readable.getReader();
    try {
      while (1) {
        const { value, done } = await reader.read();
        if (done) break;
        for (const byte of value) {
          if (frameByteIndex !== null) {
            const idx = frameByteIndex++ * 4;
            imageData.data.set(
              (COLORS[byte] ?? COLORS["invalid"]).concat(255),
              idx,
            );
            if (frameByteIndex >= 256) {
              frameByteIndex = null;
              ctx.putImageData(imageData, 0, 0);
            }
          } else if (byte === 0x1b) {
            frameByteIndex = 0;
          } else {
            if (textLogs.length < 1000) {
              textLogs += String.fromCharCode(byte);
              logs.textContent = textLogs;
            }
          }
        }
      }
    } finally {
      reader.releaseLock();
    }
  }
  console.log("exiting!");
}

const KEY_TO_DIRECTION_MAP = {
  ArrowLeft: 0,
  ArrowDown: 1,
  ArrowRight: 2,
  ArrowUp: 3,
};

async function connect(port) {
  if (!port) port = await navigator.serial.requestPort();
  await port.open({ baudRate: 115200 });
  readPump(port);
  console.log(port);

  msgs.textContent = "Running! Enjoy the game";

  const writeLock = port.writable.getWriter();
  writeLock.write(new Uint8Array(["r".charCodeAt()]));

  document.body.addEventListener("keydown", (e) => {
    if (e.repeat) return;
    if (e.key === "r") {
      writeLock.write(new Uint8Array(["r".charCodeAt()]));
      textLogs = "";
      return;
    }
    const direction = KEY_TO_DIRECTION_MAP[e.key];
    if (direction === undefined) return;
    writeLock.write(new Uint8Array([direction]));
  });
}

navigator.serial
  .getPorts()
  .then(([initialPort]) => initialPort && connect(initialPort));
