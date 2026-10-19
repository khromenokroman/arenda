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
    {path: 'tenants', title: 'Арендаторы'},
];

/** Страницы: шаблон адреса и функция отрисовки, параметры из адреса передаются по порядку. */
const ROUTES = [
    {re: /^landlords$/, render: renderLandlords},
    {re: /^landlords\/(new|\d+)$/, render: renderLandlord},
    {re: /^tenants$/, render: renderTenants},
    {re: /^tenants\/(new|\d+)$/, render: renderTenant},
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

/** Значения по умолчанию для новой карточки. */
function defaults(specs) {
    return Object.fromEntries(specs.map(s => [s.key, s.default || '']));
}

/** Поле ввода по описанию: строка, дата или выбор из списка. */
function fieldInput(spec, value) {
    const name = `name="${esc(spec.key)}"`;
    if (spec.input === 'select') {
        const options = spec.choices
            .map(c => `<option value="${esc(c.value)}"${c.value === value ? ' selected' : ''}>${esc(c.label)}</option>`)
            .join('');
        return `<select ${name}><option value="">не выбран</option>${options}</select>`;
    }
    const type = spec.input === 'date' ? 'date' : 'text';
    return `<input type="${type}" ${name} value="${esc(value)}" placeholder="${esc(spec.hint)}">`;
}

/** Кнопка заполнения из DaData рядом с полем ИНН или БИК; party - для кого ищем по ИНН (null - не искать). */
function lookupButton(spec, party) {
    if (spec.key === 'inn' && party) {
        return `<button type="button" data-lookup="party" data-for="${esc(party)}">Заполнить по ИНН</button>`;
    }
    if (spec.key === 'bik') {
        return '<button type="button" data-lookup="bank">Заполнить по БИК</button>';
    }
    return '';
}

/** Заполняет поля формы ответом /api/lookup: только существующие поля, изменённые подсвечиваются. */
async function lookupFill(form, button) {
    const isParty = button.dataset.lookup === 'party';
    const value = form.querySelector(`[name=${isParty ? 'inn' : 'bik'}]`).value.trim();
    const query = isParty ? `party?for=${encodeURIComponent(button.dataset.for)}&inn=${encodeURIComponent(value)}`
                          : `bank?bik=${encodeURIComponent(value)}`;
    button.disabled = true;
    try {
        const {fields} = await api('GET', `/api/lookup/${query}`);
        let count = 0;
        for (const [key, text] of Object.entries(fields)) {
            const input = form.querySelector(`[name=${key}]`);
            if (input && input.value !== text) {
                input.value = text;
                input.classList.add('filled');
                count++;
            }
        }
        const check = fields.signer_name_gen ? ' Проверьте должность и ФИО в родительном падеже: они составлены по правилам.' : '';
        showMessage(count === 0 ? 'Данные DaData совпадают с введёнными.' : `Заполнено из DaData полей: ${count}. Сохраните карточку.${check}`, false);
    } catch (e) {
        showMessage(e.message);
    } finally {
        button.disabled = false;
    }
}

/** Подключает кнопки «Заполнить по ...» формы. */
function bindLookup(form) {
    form.addEventListener('click', event => {
        const button = event.target.closest('button[data-lookup]');
        if (button) {
            lookupFill(form, button);
        }
    });
}

/** Форма карточки по описанию полей: разделы fieldset, подсказка в пустом поле; party - см. lookupButton. */
function fieldsForm(specs, values, party = null) {
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
            <span class="control">${fieldInput(s, values[s.key] || '')}${lookupButton(s, party)}</span></label>`).join('')}
        </fieldset>`).join('');
}

async function renderLandlord(page, idText) {
    const isNew = idText === 'new';
    const [specs, landlord] = await Promise.all([landlordFields(), isNew ? Promise.resolve(null) : api('GET', `/api/landlords/${idText}`)]);
    const values = landlord || defaults(specs);
    page.innerHTML = `<h2>${isNew ? 'Новый арендодатель' : esc(values.name_short)}</h2>
        <p><a href="#/landlords">← Арендодатели</a></p>
        <form class="card">
            ${fieldsForm(specs, values, 'landlord')}
            <div class="actions">
                <button type="submit">Сохранить</button>
                ${isNew ? '' : '<button type="button" class="danger" id="remove">Удалить</button>'}
            </div>
        </form>`;
    const form = page.querySelector('form');
    bindLookup(form);
    form.addEventListener('submit', async event => {
        event.preventDefault();
        const body = Object.fromEntries(new FormData(form));
        try {
            const saved = await api(isNew ? 'POST' : 'PUT', isNew ? '/api/landlords' : `/api/landlords/${idText}`, body);
            showMessage(`Сохранено: ${saved.name_short}`, false);
            form.querySelectorAll('.filled').forEach(i => i.classList.remove('filled'));
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
            if (!confirm(`Удалить арендодателя «${values.name_short}»?`)) {
                return;
            }
            try {
                await api('DELETE', `/api/landlords/${idText}`);
                showMessage(`Удалено: ${values.name_short}`, false);
                location.hash = '#/landlords';
            } catch (e) {
                showMessage(e.message);
            }
        });
    }
}

let tenantKinds = null;

async function tenantKindList() {
    if (!tenantKinds) {
        tenantKinds = await api('GET', '/api/tenants/kinds');
    }
    return tenantKinds;
}

async function renderTenants(page) {
    const list = await api('GET', '/api/tenants');
    const rows = list.map(t => `<tr data-id="${t.id}">
            <td><a href="#/tenants/${t.id}">${esc(t.title)}</a></td>
            <td>${esc(t.kind_label)}</td>
            <td>${esc(t.inn)}</td>
            <td>${esc(t.phone)}</td>
        </tr>`).join('');
    page.innerHTML = `<h2>Арендаторы</h2>
        <p><a class="button" href="#/tenants/new">+ арендатор</a></p>
        ${list.length === 0 ? '<p class="muted">Арендаторов пока нет.</p>' : `<table class="list">
            <thead><tr><th>Наименование</th><th>Вид</th><th>ИНН</th><th>Телефон</th></tr></thead>
            <tbody>${rows}</tbody>
        </table>`}`;
}

/** Строка таблицы оборудования; поля без name, чтобы не попасть в данные карточки. */
function equipmentRow(row) {
    return `<tr>
        <td><input data-col="name" value="${esc(row.name || '')}" placeholder="Станок для изготовления ключей"></td>
        <td><input data-col="model" value="${esc(row.model || '')}" placeholder="марка, модель"></td>
        <td><input data-col="power" value="${esc(row.power || '')}" placeholder="0,5" class="num"></td>
        <td><input data-col="qty" type="number" min="1" value="${esc(row.qty || 1)}" class="num"></td>
        <td><button type="button" class="remove-row" title="Удалить строку">✕</button></td>
    </tr>`;
}

/** Строки оборудования из таблицы; полностью пустые пропускаются. */
function equipmentRows(table) {
    return [...table.querySelectorAll('tbody tr')]
        .map(tr => Object.fromEntries([...tr.querySelectorAll('[data-col]')].map(i => [i.dataset.col, i.value.trim()])))
        .filter(r => r.name || r.model || r.power)
        .map(r => ({name: r.name, model: r.model, power: r.power, qty: Number(r.qty) || 0}));
}

/** Суммарная мощность в кВт для подписи под таблицей; нечисловые значения не считаются. */
function totalPower(rows) {
    const sum = rows.reduce((acc, r) => acc + (Number(String(r.power).replace(/\s/g, '').replace(',', '.')) || 0) * r.qty, 0);
    return sum.toLocaleString('ru-RU', {minimumFractionDigits: 2, maximumFractionDigits: 2});
}

async function renderTenant(page, idText) {
    const isNew = idText === 'new';
    const [kinds, tenant, equipment] = await Promise.all([
        tenantKindList(),
        isNew ? Promise.resolve({kind: 'ip'}) : api('GET', `/api/tenants/${idText}`),
        isNew ? Promise.resolve([]) : api('GET', `/api/tenants/${idText}/equipment`),
    ]);
    const kindOptions = kinds
        .map(k => `<option value="${esc(k.code)}"${k.code === tenant.kind ? ' selected' : ''}>${esc(k.label)}</option>`)
        .join('');
    page.innerHTML = `<h2>${isNew ? 'Новый арендатор' : esc(tenant.title)}</h2>
        <p><a href="#/tenants">← Арендаторы</a></p>
        <form class="card">
            <fieldset><legend>Вид арендатора</legend>
                <label><span>Вид *</span><select name="kind">${kindOptions}</select></label>
            </fieldset>
            <div id="kind-fields"></div>
            <fieldset><legend>Оборудование (приложение 2)</legend>
                <table class="equipment">
                    <thead><tr><th>Наименование</th><th>Марка, модель</th><th>Мощность, кВт</th><th>Кол-во</th><th></th></tr></thead>
                    <tbody>${equipment.map(equipmentRow).join('')}</tbody>
                </table>
                <p><button type="button" id="add-row">+ строка</button>
                    <span class="muted" id="total-power"></span></p>
            </fieldset>
            <div class="actions">
                <button type="submit">Сохранить</button>
                ${isNew ? '' : '<button type="button" class="danger" id="remove">Удалить</button>'}
            </div>
        </form>`;
    const form = page.querySelector('form');
    const kindSelect = form.querySelector('select[name=kind]');
    const box = page.querySelector('#kind-fields');
    let values = {...tenant};
    const drawFields = () => {
        const kind = kinds.find(k => k.code === kindSelect.value);
        box.innerHTML = fieldsForm(kind.fields, values, kind.code === 'person' ? null : kind.code);
    };
    kindSelect.addEventListener('change', () => {
        // введённое сохраняется: общие поля видов (ФИО, ИНН, банк) переходят в новую форму
        values = {...values, ...Object.fromEntries(new FormData(form))};
        drawFields();
    });
    drawFields();
    bindLookup(form);
    const table = form.querySelector('table.equipment');
    const showPower = () => {
        const rows = equipmentRows(table);
        form.querySelector('#total-power').textContent = rows.length === 0 ? 'не используется' : `суммарная мощность ${totalPower(rows)} кВт`;
    };
    form.querySelector('#add-row').addEventListener('click', () => {
        table.querySelector('tbody').insertAdjacentHTML('beforeend', equipmentRow({}));
        table.querySelector('tbody tr:last-child input').focus();
    });
    table.addEventListener('click', event => {
        if (event.target.closest('.remove-row')) {
            event.target.closest('tr').remove();
            showPower();
        }
    });
    table.addEventListener('input', showPower);
    showPower();
    form.addEventListener('submit', async event => {
        event.preventDefault();
        const body = Object.fromEntries(new FormData(form));
        try {
            const saved = await api(isNew ? 'POST' : 'PUT', isNew ? '/api/tenants' : `/api/tenants/${idText}`, body);
            try {
                await api('PUT', `/api/tenants/${saved.id}/equipment`, equipmentRows(table));
            } catch (e) {
                // карточка уже сохранена: открываем её, чтобы исправить оборудование
                if (isNew) {
                    location.hash = `#/tenants/${saved.id}`;
                }
                throw new Error(`Карточка сохранена, оборудование нет: ${e.message}`);
            }
            showMessage(`Сохранено: ${saved.title}`, false);
            form.querySelectorAll('.filled').forEach(i => i.classList.remove('filled'));
            if (isNew) {
                location.hash = `#/tenants/${saved.id}`;
            } else {
                page.querySelector('h2').textContent = saved.title;
            }
        } catch (e) {
            showMessage(e.message);
        }
    });
    const remove = page.querySelector('#remove');
    if (remove) {
        remove.addEventListener('click', async () => {
            if (!confirm(`Удалить арендатора «${tenant.title}»?`)) {
                return;
            }
            try {
                await api('DELETE', `/api/tenants/${idText}`);
                showMessage(`Удалено: ${tenant.title}`, false);
                location.hash = '#/tenants';
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
