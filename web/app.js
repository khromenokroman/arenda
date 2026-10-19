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

/** Разделы интерфейса: адрес после # и заголовок. */
const PAGES = [
    {hash: 'home', title: 'Главная', render: renderHome},
];

function renderHome(page) {
    page.innerHTML = '<p>Здесь будут справочники арендодателей, арендаторов и договоры.</p>';
}

function render() {
    const name = location.hash.slice(1) || 'home';
    const current = PAGES.find(p => p.hash === name) || PAGES[0];
    document.getElementById('menu').innerHTML = PAGES
        .map(p => `<a href="#${p.hash}"${p === current ? ' class="active"' : ''}>${p.title}</a>`)
        .join('');
    current.render(document.getElementById('page'));
}

async function showVersion() {
    const footer = document.getElementById('version');
    try {
        const v = await api('GET', '/api/version');
        footer.textContent = `arenda ${v.version}, схема базы ${v.schema}`;
    } catch (e) {
        footer.innerHTML = `<span class="error">${e.message}</span>`;
    }
}

window.addEventListener('hashchange', render);
render();
showVersion();
