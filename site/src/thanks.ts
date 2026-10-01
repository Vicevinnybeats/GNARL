import { createCheckoutScene } from './checkoutScene';

import './styles.css';
import './checkout.css';

/*
 * The page Stripe returns a buyer to: their licence key, then the downloads.
 *
 * Stripe's Payment Link is set to return here with ?session_id=
 * {CHECKOUT_SESSION_ID}. The licence service (backend/src/stripe.ts) issues
 * the key when Stripe's webhook reaches it, which can be a moment AFTER the
 * buyer arrives, so "pending" is polled, not reported as a failure.
 */

const stage = document.querySelector<HTMLCanvasElement>('#stage');
if (stage) createCheckoutScene(stage);

const keyNode = document.querySelector<HTMLElement>('#key');
const copy = document.querySelector<HTMLButtonElement>('#copy');
const note = document.querySelector<HTMLElement>('#key-note');

const api = document.querySelector<HTMLMetaElement>('meta[name="gnarl-licence-api"]')?.content.trim() ?? '';
const session = new URLSearchParams(location.search).get('session_id') ?? '';

// Every 2 s for a minute: a webhook is normally seconds behind the buyer.
const POLL_MS = 2000;
const POLL_TRIES = 30;

function show(text: string, isKey = false): void {
  if (keyNode) keyNode.textContent = text;
  if (copy) copy.hidden = !isKey;
}

async function fetchKey(): Promise<void> {
  if (!api) {
    show('The shop is not open yet.');
    if (note) note.textContent = 'Payments start with the 1.0 release. If you were charged, reply to your Stripe receipt.';
    return;
  }
  if (!/^cs_[A-Za-z0-9_]+$/.test(session)) {
    show('No purchase found on this page.');
    if (note) note.textContent = 'Open the link from your Stripe receipt, or reply to it and we will send your key.';
    return;
  }
  for (let attempt = 0; attempt < POLL_TRIES; attempt++) {
    try {
      const response = await fetch(`${api.replace(/\/$/, '')}/licence?session_id=${encodeURIComponent(session)}`, { cache: 'no-store' });
      if (response.ok) {
        const body = (await response.json()) as { key?: string; status?: string };
        if (body.key) {
          show(body.key, true);
          return;
        }
      } else if (response.status !== 404 && response.status !== 503) {
        break;
      }
    } catch {
      // The network blinked: try again on the next tick.
    }
    await new Promise((resolve) => setTimeout(resolve, POLL_MS));
  }
  show('Your key is on its way.');
  if (note) note.textContent = 'It is taking longer than usual. Reload this page in a minute, or reply to your Stripe receipt.';
}

copy?.addEventListener('click', () => {
  const key = keyNode?.textContent ?? '';
  navigator.clipboard?.writeText(key).then(
    () => {
      if (copy) copy.textContent = 'COPIED';
    },
    () => {
      // No clipboard here: select the key so a long-press copies it.
      if (keyNode) getSelection()?.selectAllChildren(keyNode);
    },
  );
});

void fetchKey();
