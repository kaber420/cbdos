// Lector de firmwares desde GitHub Releases + manifiesto .ini/.txt.
// Convención de assets (merged-bin único @0x0):
//   cbdos-<board_id>-<soc>-<version>-merged.bin
//   cbdos-<board_id>-<soc>-<version>.ini   (opcional, misma base)
// Filtro: el asset debe contener el soc y, si existe, el board_id.
import type { Board } from './boards';

export const GH_REPO = 'kaber420/CBD-os';
export const GH_API = `https://api.github.com/repos/${GH_REPO}/releases?per_page=20`;

export interface FirmwareFile {
  fileName: string;
  version: string;
  boardId: string | null;
  soc: string | null;
  size: number;
  downloadUrl: string;
  iniUrl: string | null;
  ini: Record<string, string> | null;
  releaseTag: string;
  releaseName: string;
  publishedAt: string;
  notes: string;
}

interface GhAsset {
  name: string;
  size: number;
  browser_download_url: string;
}
export interface GhRelease {
  tag_name: string;
  name: string;
  published_at: string;
  body: string;
  assets: GhAsset[];
}

const VER_RE = /v?(\d+\.\d+\.\d+(?:[-._][0-9A-Za-z.]+)?)/;

export function parseVersion(tagOrName: string): string {
  const m = tagOrName.match(VER_RE);
  return m ? m[1] : tagOrName;
}

function parseIni(text: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith(';') || line.startsWith('#') || line.startsWith('[')) continue;
    const eq = line.indexOf('=');
    if (eq < 0) continue;
    out[line.slice(0, eq).trim().toLowerCase()] = line.slice(eq + 1).trim();
  }
  return out;
}

export async function fetchIni(url: string): Promise<Record<string, string> | null> {
  try {
    const r = await fetch(url);
    if (!r.ok) return null;
    return parseIni(await r.text());
  } catch {
    return null;
  }
}

/** Firmwares de un release que calzan con la placa (soc + board_id). */
export function matchBoardAssets(release: GhRelease, board: Board): GhAsset[] {
  const soc = board.soc.toLowerCase();
  const bid = board.boardId.toLowerCase();
  const bins = release.assets.filter((a) => a.name.toLowerCase().endsWith('.bin'));
  const exact = bins.filter((a) => {
    const n = a.name.toLowerCase();
    return n.includes(soc) && n.includes(bid);
  });
  if (exact.length > 0) return exact;
  // Fallback: genérico por SoC (para futuros bins universales esp32-p4).
  return bins.filter((a) => a.name.toLowerCase().includes(soc));
}

/** Una sola lectura cruda a la API de Releases (para cachear y filtrar en local). */
export async function fetchReleasesRaw(): Promise<{
  releases: GhRelease[];
  error: string | null;
}> {
  try {
    const r = await fetch(GH_API, { headers: { Accept: 'application/vnd.github+json' } });
    if (r.status === 403) {
      return { releases: [], error: 'Límite de la API de GitHub alcanzado (403). Se reintenta en ~1 h; mientras tanto el flasheo manual sigue disponible abajo.' };
    }
    if (!r.ok) {
      return { releases: [], error: `GitHub API ${r.status}: no se pudo leer Releases.` };
    }
    return { releases: (await r.json()) as GhRelease[], error: null };
  } catch {
    return { releases: [], error: 'Sin conexión a GitHub. Revisa tu red (offline-first: el flasheo manual sigue disponible abajo).' };
  }
}

/** Convierte releases crudos en lista de firmwares para una placa (sin tocar la API). */
export async function buildFirmwareList(
  releases: GhRelease[],
  board: Board,
): Promise<FirmwareFile[]> {
  const files: FirmwareFile[] = [];
  for (const rel of releases) {
    for (const a of matchBoardAssets(rel, board)) {
      const lower = a.name.toLowerCase();
      const base = a.name.replace(/\.bin$/i, '');
      const iniAsset = rel.assets.find(
        (x) =>
          x.name.toLowerCase() === `${base}.ini`.toLowerCase() ||
          x.name.toLowerCase() === `${base}.txt`.toLowerCase(),
      );
      // El .ini vive en el CDN de descargas, no cuenta para el rate-limit de la API.
      const ini = iniAsset ? await fetchIni(iniAsset.browser_download_url) : null;
      files.push({
        fileName: a.name,
        version: ini?.['version'] ?? parseVersion(a.name.includes('v') ? a.name : rel.tag_name),
        boardId: lower.includes(board.boardId.toLowerCase()) ? board.boardId : (ini?.['board_id'] ?? null),
        soc: lower.includes(board.soc.toLowerCase()) ? board.soc : (ini?.['soc'] ?? null),
        size: a.size,
        downloadUrl: a.browser_download_url,
        iniUrl: iniAsset ? iniAsset.browser_download_url : null,
        ini,
        releaseTag: rel.tag_name,
        releaseName: rel.name || rel.tag_name,
        publishedAt: rel.published_at,
        notes: rel.body || '',
      });
    }
  }
  return files;
}

export async function fetchFirmwares(board: Board): Promise<{
  files: FirmwareFile[];
  error: string | null;
}> {
  const { releases, error } = await fetchReleasesRaw();
  if (error) return { files: [], error };
  return { files: await buildFirmwareList(releases, board), error: null };
}

export function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / 1024 / 1024).toFixed(2)} MB`;
}
