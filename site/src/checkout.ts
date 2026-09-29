import './styles.css';
import './checkout.css';

/**
 * The checkout page's one job: do not lie about being able to take money.
 *
 * A PAYMENT LINK IS A URL AND NOTHING ELSE. Stripe Payment Links are created
 * in the Stripe dashboard and are just a https://buy.stripe.com/... address,
 * so wiring this up is pasting one string into the `data-payment-link`
 * attribute in checkout.html. No keys, no secrets, no server: a publishable
 * key would be safe to ship and a secret key would not, and the way to make
 * that distinction impossible to get wrong is to have neither.
 *
 * UNTIL THAT STRING EXISTS THE BUTTON IS INERT, and says so. A button that
 * looks live and does nothing is the worst thing this page can do — it is the
 * one page where the whole interaction is trust, and a dead buy button reads
 * as a broken shop rather than as an unfinished one.
 */
const button = document.querySelector<HTMLAnchorElement>('#pay');
const note = document.querySelector<HTMLElement>('#pay-note');

const link = button?.dataset.paymentLink?.trim() ?? '';

if (button && link.length > 0) {
  button.href = link;
  button.removeAttribute('aria-disabled');
  button.textContent = 'PAY €49 — SECURE CHECKOUT';
  button.rel = 'noreferrer';

  //  The notice was describing a state that no longer applies.
  note?.remove();
} else if (button) {
  /*  Inert, and not merely styled as inert. A disabled-looking <a> that still
      navigates on Enter is a trap for anyone using a keyboard. */
  button.addEventListener('click', (event) => event.preventDefault());
  button.tabIndex = -1;
}
