/*
 * Preset sync (docs/design/phase8-01-sync.md): the CLOUD list in the preset
 * sheet, one list of patches for the plugin and the phone, opened by a sync
 * code (SYNC-XXXX-XXXX-XXXX-XXXX). No account: the code is the identity, as
 * the licence key is for licensing; write it down or open it on the other
 * device to see the same list.
 *
 * The server is backend/src/sync.ts. Its address is baked in at build time
 * (VITE_GNARL_SYNC_URL, the release workflow's GNARL_BACKEND_URL); a build
 * without one shows no CLOUD section and makes no request.
 */

// A page may also name one (window.__GNARL_SYNC_URL__, set before the page
// loads): tests/sync.test.mjs points it at a stand-in server that way.
const configured =
  (import.meta.env.VITE_GNARL_SYNC_URL as string | undefined) ??
  (window as { __GNARL_SYNC_URL__?: string }).__GNARL_SYNC_URL__ ??
  '';
export const SYNC_URL: string = configured.replace(/\/+$/, '');

const CODE_KEY = 'gnarl.sync';

export function syncAvailable(): boolean {
  return SYNC_URL.length > 0;
}

export function storedCode(): string | null {
  try {
    return localStorage.getItem(CODE_KEY);
  } catch {
    return null;
  }
}

export function storeCode(code: string | null): void {
  try {
    if (code === null) localStorage.removeItem(CODE_KEY);
    else localStorage.setItem(CODE_KEY, code);
  } catch {
    // A private window keeps the code for the visit only.
  }
}

/** How a code is shown: grouped, whatever was typed (the server reads either). */
export function formatCode(raw: string): string | null {
  let symbols = raw.toUpperCase().replace(/[^0-9A-Z]/g, '');
  if (symbols.length === 20 && symbols.startsWith('SYNC')) symbols = symbols.slice(4);
  if (!/^[2-9A-HJ-NP-Z]{16}$/.test(symbols)) return null;
  return `SYNC-${symbols.match(/.{4}/g)?.join('-') ?? symbols}`;
}

export interface CloudPatch {
  name: string;
  updated_at: string;
  size: number;
}

/** A failure in words for the page. */
export class SyncError extends Error {}

async function request<T>(path: string, init: RequestInit = {}, code: string | null = storedCode()): Promise<T> {
  const headers = new Headers(init.headers);
  if (code) headers.set('authorization', `Bearer ${code}`);
  let response: Response;
  try {
    response = await fetch(`${SYNC_URL}${path}`, { ...init, headers });
  } catch {
    throw new SyncError('No connection to the cloud. Your patches here are untouched.');
  }
  if (response.status === 204) return undefined as T;
  const body = (await response.json().catch(() => ({}))) as { error?: string } & T;
  if (response.ok) return body;
  if (response.status === 404 && path === '/sync') throw new SyncError('That sync code is not known. Check it, or make a new one.');
  if (response.status >= 500) throw new SyncError('The cloud is not answering right now. Try again in a minute.');
  throw new SyncError(body.error ? `Cloud: ${body.error}.` : `Cloud error ${response.status}.`);
}

/** A new, empty list; its code is kept on this device. */
export async function newCode(): Promise<string> {
  const { code } = await request<{ code: string }>('/sync/new', { method: 'POST' }, null);
  storeCode(code);
  return code;
}

/** Open an existing list by its code: checked first, then kept. */
export async function useCode(typed: string): Promise<string> {
  const code = formatCode(typed);
  if (code === null) throw new SyncError('A sync code is SYNC- and 16 letters and digits.');
  await request<unknown>('/sync', {}, code);
  storeCode(code);
  return code;
}

export async function listCloud(): Promise<CloudPatch[]> {
  return (await request<{ presets: CloudPatch[] }>('/sync')).presets;
}

export async function readCloud(name: string): Promise<string> {
  return (await request<{ patch: string }>(`/sync/p/${encodeURIComponent(name)}`)).patch;
}

export async function writeCloud(name: string, patch: string): Promise<void> {
  await request<unknown>(`/sync/p/${encodeURIComponent(name)}`, {
    method: 'PUT',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify({ patch }),
  });
}

export async function deleteCloud(name: string): Promise<void> {
  await request<unknown>(`/sync/p/${encodeURIComponent(name)}`, { method: 'DELETE' });
}
