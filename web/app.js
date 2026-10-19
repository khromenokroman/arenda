'use strict';

/** Запрос к API: JSON в ответе, при ошибке исключение с текстом сервера. */
async function api(method, path, body) {
    const res = await fetch(path, {
        method,
        headers: body === undefined ? {} : {'Content-Type': 'application/json'},
        body: body === undefined ? undefined : JSON.stringify(body),
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok) {
        throw new Error(data.error || `ошибка ${res.status}`);
    }
    return data;
}

/** Экранирует текст для вставки в HTML. */
function esc(text) {
    return String(text).replace(/[&<>"']/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[c]));
}

/** Сообщение под шапкой: ошибка или успех; пустой текст скрывает. */
function showMessage(text, isError = true) {
    const box = document.getElementById('message');
    box.textContent = text;
    box.className = text ? (isError ? 'error' : 'ok') : 'hidden';
}

/** Разделы меню: адрес после #/ и заголовок. */
const MENU = [
    {path: 'landlords', title: 'Арендодатели'},
];

/** Страницы: шаблон адреса и функция отрисовки, параметры из адреса передаются по порядку. */
const ROUTES = [
    {re: /^landlords$/, render: renderLandlords},
    {re: /^landlords\/(new|\d+)$/, render: renderLandlord},
];

let landlordSpecs = null;

async function landlordFields() {
    if (!landlordSpecs) {
        landlordSpecs = await api('GET', '/api/landlords/fields');
    }
    return landlordSpecs;
}

async function renderLandlords(page) {
    const list = await api('GET', '/api/landlords');
    const rows = list.map(l => `<tr data-id="${l.id}">
            <td><a href="#/landlords/${l.id}">${esc(l.name_short)}</a></td>
            <td>${esc(l.inn)}</td>
            <td>${esc(l.object_name_gen)}</td>
        </tr>`).join('');
    page.innerHTML = `<h2>Арендодатели</h2>
        <p><a class="button" href="#/landlords/new">+ арендодатель</a></p>
        ${list.length === 0 ? '<p class="muted">Арендодателей пока нет.</p>' : `<table class="list">
            <thead><tr><th>Наименование</th><th>ИНН</th><th>Объект</th></tr></thead>
            <tbody>${rows}</tbody>
        </table>`}`;
}

/** Форма карточки по описанию полей: разделы fieldset, подсказка в пустом поле. */
function fieldsForm(specs, values) {
    const groups = [];
    for (const spec of specs) {
        let group = groups.find(g => g.name === spec.group);
        if (!group) {
            group = {name: spec.group, specs: []};
            groups.push(group);
        }
        group.specs.push(spec);
    }
    return groups.map(g => `<fieldset><legend>${esc(g.name)}</legend>
        ${g.specs.map(s => `<label><span>${esc(s.label)}${s.required ? ' *' : ''}</span>
            <input name="${esc(s.key)}" value="${esc(values[s.key] || '')}" placeholder="${esc(s.hint)}"></label>`).join('')}
        </fieldset>`).join('');
}

async function renderLandlord(page, idText) {
    const isNew = idText === 'new';
    const [specs, landlord] = await Promise.all([landlordFields(), isNew ? Promise.resolve({}) : api('GET', `/api/landlords/${idText}`)]);
    page.innerHTML = `<h2>${isNew ? 'Новый арендодатель' : esc(landlord.name_short)}</h2>
        <p><a href="#/landlords">← Арендодатели</a></p>
        <form class="card">
            ${fieldsForm(specs, landlord)}
            <div class="actions">
                <button type="submit">Сохранить</button>
                ${isNew ? '' : '<button type="button" class="danger" id="remove">Удалить</button>'}
            </div>
        </form>`;
    const form = page.querySelector('form');
    form.addEventListener('submit', async event => {
        event.preventDefault();
        const body = Object.fromEntries(new FormData(form));
        try {
            const saved = await api(isNew ? 'POST' : 'PUT', isNew ? '/api/landlords' : `/api/landlords/${idText}`, body);
            showMessage(`Сохранено: ${saved.name_short}`, false);
            if (isNew) {
                location.hash = `#/landlords/${saved.id}`;
            } else {
                page.querySelector('h2').textContent = saved.name_short;
            }
        } catch (e) {
            showMessage(e.message);
        }
    });
    const remove = page.querySelector('#remove');
    if (remove) {
        remove.addEventListener('click', async () => {
            if (!confirm(`Удалить арендодателя «${landlord.name_short}»?`)) {
                return;
            }
            try {
                await api('DELETE', `/api/landlords/${idText}`);
                showMessage(`Удалено: ${landlord.name_short}`, false);
                location.hash = '#/landlords';
            } catch (e) {
                showMessage(e.message);
            }
        });
    }
}

async function render() {
    const path = location.hash.replace(/^#\/?/, '') || MENU[0].path;
    document.getElementById('menu').innerHTML = MENU
        .map(m => `<a href="#/${m.path}"${path.split('/')[0] === m.path ? ' class="active"' : ''}>${m.title}</a>`)
        .join('');
    const page = document.getElementById('page');
    const route = ROUTES.find(r => r.re.test(path));
    if (!route) {
        page.innerHTML = '<h2>Нет такой страницы</h2>';
        return;
    }
    try {
        await route.render(page, ...path.match(route.re).slice(1));
    } catch (e) {
        page.innerHTML = '<h2>Ошибка</h2>';
        showMessage(e.message);
    }
}

async function showVersion() {
    const footer = document.getElementById('version');
    try {
        const v = await api('GET', '/api/version');
        footer.textContent = `arenda ${v.version}, схема базы ${v.schema}`;
    } catch (e) {
        footer.innerHTML = `<span class="error">${esc(e.message)}</span>`;
    }
}

window.addEventListener('hashchange', () => {
    showMessage('');
    render();
});
render();
showVersion();
