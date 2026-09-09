// Catálogo de placas CBDos (v1: sin DTB — solo identidad soc + board_id).
// A futuro cada entrada crecerá con pines/drivers/periféricos (tipo DTB);
// la web y el filtro de Releases ya están preparados para ese campo extra.
export interface Board {
  boardId: string; // ej: "jc4880p443" — coincide con CBDOS:BOARD= y con el nombre del asset
  soc: string; // ej: "esp32-p4" — coincide con SOC= y con el nombre del asset
  name: string;
  short: string;
  flashSize: string; // para esptool-js FlashSizeValues
  baud: number;
  bspPath: string;
}

export const BOARDS: Board[] = [
  {
    boardId: 'jc4880p443',
    soc: 'esp32-p4',
    name: 'Guition JC4880P443C',
    short: 'JC4880 (P4)',
    flashSize: '16MB',
    baud: 921600,
    bspPath: 'bsp/esp32_p4_jc4880',
  },
  {
    boardId: 'jc3248w535',
    soc: 'esp32-s3',
    name: 'Guition JC3248W535',
    short: 'JC3248 (S3)',
    flashSize: '16MB',
    baud: 921600,
    bspPath: 'bsp/esp32_s3_jc3248',
  },
];

export function findBoard(boardId: string): Board {
  return BOARDS.find((b) => b.boardId === boardId) ?? BOARDS[0];
}
