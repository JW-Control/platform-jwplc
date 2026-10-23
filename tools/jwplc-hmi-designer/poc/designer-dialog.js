(() => {
  'use strict';

  // Avisos y confirmaciones con el aspecto del Designer, en lugar de los
  // cuadros genéricos de Windows/Chromium (window.confirm/alert/prompt).
  //
  //   await JWPLCDialog.confirm(mensaje, { okLabel, cancelLabel, danger })  -> true/false
  //   await JWPLCDialog.alert(mensaje)                                       -> undefined
  //   await JWPLCDialog.prompt(mensaje, valorInicial)                        -> texto o null
  //   await JWPLCDialog.choose(mensaje, [{ id, label, primary }], { cancelId }) -> id elegido
  //
  // Son asíncronas: quien las usa hace «await». Enter acepta, Esc cancela; el
  // foco queda dentro del cuadro mientras está abierto. Los cuadros de abrir y
  // guardar archivos siguen siendo los nativos.

  if (window.JWPLCDialog) return;

  const STYLE = `
.jw-dialog-backdrop {
  position: fixed; inset: 0; z-index: 12000; display: grid; place-items: center; padding: 16px;
  background: rgba(4, 9, 14, 0.62); backdrop-filter: blur(2px); animation: jw-dialog-fade .12s ease-out;
}
.jw-dialog {
  width: min(460px, 100%); max-height: calc(100vh - 32px); display: flex; flex-direction: column;
  background: #0f1b23; border: 1px solid #2c4252; border-radius: 10px; color: #dce8ef;
  box-shadow: 0 18px 50px rgba(0, 0, 0, 0.55); font: 13px/1.5 'Segoe UI', sans-serif;
  animation: jw-dialog-pop .14s ease-out;
}
.jw-dialog-head {
  display: flex; align-items: center; gap: 8px; padding: 12px 16px 0;
  font-size: 13px; font-weight: 600; color: #ff9a43;
}
.jw-dialog-head::before {
  content: ''; width: 8px; height: 8px; border-radius: 50%; background: currentColor; flex: none;
}
.jw-dialog[data-kind="danger"] .jw-dialog-head { color: #f45f65; }
.jw-dialog-body { padding: 8px 16px 4px; overflow: auto; white-space: pre-wrap; overflow-wrap: anywhere; }
.jw-dialog-input {
  display: block; width: 100%; margin-top: 10px; padding: 7px 9px; border: 1px solid #2c4252; border-radius: 6px;
  background: #081018; color: #e7eef5; font: 13px 'Cascadia Code', Consolas, monospace;
}
.jw-dialog-input:focus { outline: none; border-color: #ff8a2a; box-shadow: 0 0 0 2px rgba(255, 138, 42, 0.22); }
.jw-dialog-actions { display: flex; justify-content: flex-end; gap: 8px; padding: 14px 16px 14px; }
.jw-dialog-actions button {
  min-width: 92px; padding: 6px 14px; border: 1px solid #2c4252; border-radius: 6px;
  background: #132330; color: #dce8ef; font: 600 12px 'Segoe UI', sans-serif; cursor: pointer;
}
.jw-dialog-actions button:hover { background: #18303f; border-color: #3b5668; }
.jw-dialog-actions button:focus-visible { outline: 2px solid rgba(255, 138, 42, 0.7); outline-offset: 1px; }
.jw-dialog-actions button.is-primary {
  background: linear-gradient(180deg, rgba(255, 138, 42, 0.30) 0%, rgba(255, 138, 42, 0.14) 100%);
  border-color: rgba(255, 138, 42, 0.8); color: #fff;
}
.jw-dialog-actions button.is-primary:hover { border-color: #ff9a43; background: rgba(255, 138, 42, 0.36); }
.jw-dialog[data-kind="danger"] .jw-dialog-actions button.is-primary {
  background: rgba(244, 95, 101, 0.22); border-color: rgba(244, 95, 101, 0.85);
}
@keyframes jw-dialog-fade { from { opacity: 0; } }
@keyframes jw-dialog-pop { from { opacity: 0; transform: translateY(6px) scale(.98); } }
`;

  function ensureStyle() {
    if (document.getElementById('jwDialogStyle')) return;
    const style = document.createElement('style');
    style.id = 'jwDialogStyle';
    style.textContent = STYLE;
    document.head.appendChild(style);
  }

  // Un cuadro a la vez: los siguientes esperan su turno.
  let queue = Promise.resolve();

  function open({ kind, title, message, okLabel, cancelLabel, input, danger, choices, cancelId }) {
    const run = () => new Promise((resolve) => {
      ensureStyle();
      const previousFocus = document.activeElement;
      const backdrop = document.createElement('div');
      backdrop.className = 'jw-dialog-backdrop';
      const box = document.createElement('div');
      box.className = 'jw-dialog';
      box.dataset.kind = danger ? 'danger' : kind;
      box.setAttribute('role', kind === 'alert' ? 'alertdialog' : 'dialog');
      box.setAttribute('aria-modal', 'true');

      const head = document.createElement('div');
      head.className = 'jw-dialog-head';
      head.textContent = title;
      const body = document.createElement('div');
      body.className = 'jw-dialog-body';
      body.id = `jwDialogBody${Date.now()}`;
      body.textContent = String(message ?? '');
      box.setAttribute('aria-labelledby', body.id);

      let field = null;
      if (input !== undefined) {
        field = document.createElement('input');
        field.className = 'jw-dialog-input';
        field.type = 'text';
        field.value = String(input ?? '');
        field.spellcheck = false;
        body.appendChild(field);
      }

      const actions = document.createElement('div');
      actions.className = 'jw-dialog-actions';
      const ok = document.createElement('button');
      ok.type = 'button';
      ok.className = 'is-primary';
      ok.dataset.jwDialog = 'ok';
      ok.textContent = okLabel;
      let cancel = null;
      // choose: botones libres; el último con primary es el de Enter.
      const extra = (choices || []).map((choice) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.dataset.jwDialog = choice.id;
        button.textContent = choice.label;
        if (choice.primary) button.className = 'is-primary';
        button.addEventListener('click', () => finish(choice.id));
        return button;
      });
      if (choices) {
        actions.append(...extra);
      } else if (cancelLabel) {
        cancel = document.createElement('button');
        cancel.type = 'button';
        cancel.dataset.jwDialog = 'cancel';
        cancel.textContent = cancelLabel;
        actions.appendChild(cancel);
      }
      if (!choices) actions.appendChild(ok);
      const primary = choices ? (extra.filter((b) => b.className === 'is-primary').pop() || extra[extra.length - 1]) : ok;
      box.append(head, body, actions);
      backdrop.appendChild(box);
      document.body.appendChild(backdrop);

      const finish = (accepted) => {
        document.removeEventListener('keydown', onKey, true);
        backdrop.remove();
        try { previousFocus?.focus?.(); } catch (_) {}
        if (kind === 'choose') resolve(accepted === true ? primary.dataset.jwDialog : accepted === false ? cancelId : accepted);
        else if (kind === 'prompt') resolve(accepted ? field.value : null);
        else resolve(kind === 'alert' ? undefined : accepted);
      };

      // Captura en document: ningún atajo del editor se dispara con el cuadro abierto.
      function onKey(event) {
        if (event.key === 'Escape') {
          event.preventDefault(); event.stopImmediatePropagation();
          finish(kind === 'alert');
        } else if (event.key === 'Enter' && (event.target === field || !event.target.closest?.('.jw-dialog-actions'))) {
          event.preventDefault(); event.stopImmediatePropagation();
          finish(true);
        } else if (event.key === 'Tab') {
          const focusable = choices ? extra : [field, cancel, ok].filter(Boolean);
          const index = focusable.indexOf(document.activeElement);
          event.preventDefault(); event.stopImmediatePropagation();
          focusable[(index + (event.shiftKey ? -1 : 1) + focusable.length) % focusable.length].focus();
        } else {
          event.stopImmediatePropagation();
        }
      }
      document.addEventListener('keydown', onKey, true);
      ok.addEventListener('click', () => finish(true));
      cancel?.addEventListener('click', () => finish(false));
      backdrop.addEventListener('mousedown', (event) => {
        if (event.target === backdrop) event.preventDefault(); // no se cierra por accidente
      });

      if (field) { field.focus(); field.select(); } else primary.focus();
    });
    const result = queue.then(run, run);
    queue = result.catch(() => {});
    return result;
  }

  const TITLE = 'JWPLC HMI Designer';

  window.JWPLCDialog = {
    confirm: (message, options = {}) => open({
      kind: 'confirm', title: options.title || TITLE, message,
      okLabel: options.okLabel || 'Aceptar', cancelLabel: options.cancelLabel || 'Cancelar', danger: options.danger
    }),
    alert: (message, options = {}) => open({
      kind: 'alert', title: options.title || TITLE, message, okLabel: options.okLabel || 'Entendido'
    }),
    choose: (message, choices, options = {}) => open({
      kind: 'choose', title: options.title || TITLE, message, choices, danger: options.danger,
      cancelId: options.cancelId ?? null
    }),
    prompt: (message, value = '', options = {}) => open({
      kind: 'prompt', title: options.title || TITLE, message, input: value,
      okLabel: options.okLabel || 'Aceptar', cancelLabel: options.cancelLabel || 'Cancelar'
    })
  };
})();
