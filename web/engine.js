/* Keep browser rendering separate from the deterministic C authority. */
export async function loadEngine(url, onFeedback = () => {}) {
  let instance;
  const decoder = new TextDecoder();
  const text = pointer => {
    const bytes = new Uint8Array(instance.exports.memory.buffer);
    let end = pointer;
    while (end < bytes.length && bytes[end]) end++;
    return decoder.decode(bytes.subarray(pointer, end));
  };
  const imports = { env: { feedback(type, actor, target, row, col, pointer, round) {
    onFeedback({ type, actor, target, row, col, round, message: text(pointer) });
  } } };
  const response = await fetch(url);
  if (!response.ok) throw Error(`WebAssembly download failed (${response.status}).`);
  ({ instance } = await WebAssembly.instantiate(await response.arrayBuffer(), imports));
  return {
    start: (mission, seed, specialist, flags) => instance.exports.start(mission, seed, specialist, flags),
    command: (op, a = 0, b = 0, c = 0, d = -1) => instance.exports.command(op, a, b, c, d),
    snapshot: viewer => JSON.parse(text(instance.exports.snapshot(viewer))),
    error: () => text(instance.exports.error()),
    name: id => text(instance.exports.mission_name(id)),
    objective: id => text(instance.exports.mission_objective(id))
  };
}
