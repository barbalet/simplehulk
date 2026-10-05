/* Mirror the public C tile classifiers; the core owns collision and sight. */
export const CLOSED_DOOR = '=';
export const OPEN_DOOR = '/';
export function isWall(tile) { return ['+', '|', '-', '#'].includes(tile); }
export function isFloor(tile) { return typeof tile === 'string' && /^[./A-Z1-3]$/.test(tile); }
export function wallMask(map, row, col) {
  let mask = 0;
  for (const [dr, dc, bit] of [[-1, 0, 1], [0, 1, 2], [1, 0, 4], [0, -1, 8]]) {
    if (isWall(map[row + dr]?.[col + dc])) mask |= bit;
  }
  return mask;
}
