'use strict';

/** Запрос к API: JSON в ответе, при ошибке исключение с текстом сервера. */
async function api(method, path, body) {
    const res = await fetch(path, {
        method,
        headers: body === undefined ? {} : {'Content-Type': 'application/json'},
        body: body === undefined ? undefined : JSON.stringify(body),
    });
    const data = await res.json().catch(() => ({}));
    if (res.status === 401 && !path.startsWith('/api/auth')) {
        // сессия истекла или закрыта: снова форма входа, после входа та же страница
        currentUser = null;
        renderLogin();
        throw new Error(data.error || 'нужно войти');
    }
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

/** Вошедший пользователь {id, login, name, role} или null. */
let currentUser = null;

/** Разделы меню: адрес после #/ и заголовок; admin - только для администратора. */
const MENU = [
    {path: 'contracts', title: 'Договоры'},
    {path: 'landlords', title: 'Арендодатели'},
    {path: 'tenants', title: 'Арендаторы'},
    {path: 'users', title: 'Пользователи', admin: true},
    {path: 'backups', title: 'Копии', admin: true},
];

/** Страницы: шаблон адреса и функция отрисовки, параметры из адреса передаются по порядку. */
const ROUTES = [
    {re: /^contracts$/, render: renderContracts},
    {re: /^contracts\/new$/, render: renderContractsNew},
    {re: /^contracts\/(\d+)$/, render: renderContract},
    {re: /^landlords$/, render: renderLandlords},
    {re: /^landlords\/(new|\d+)$/, render: renderLandlord},
    {re: /^tenants$/, render: renderTenants},
    {re: /^tenants\/(new|\d+)$/, render: renderTenant},
    {re: /^users$/, render: renderUsers},
    {re: /^password$/, render: renderPassword},
    {re: /^backups$/, render: renderBackups},
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
            form.querySelectorAll('.filled').forEach(i => i.classList.remove('filled'));
            if (isNew) {
                navigate(`#/landlords/${saved.id}`, `Сохранено: ${saved.name_short}`);
            } else {
                page.querySelector('h2').textContent = saved.name_short;
                showMessage(`Сохранено: ${saved.name_short}`, false);
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
                navigate('#/landlords', `Удалено: ${values.name_short}`);
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
                const text = `Карточка сохранена, оборудование нет: ${e.message}`;
                if (isNew) {
                    // карточка уже есть: открываем её, чтобы исправить оборудование
                    navigate(`#/tenants/${saved.id}`, text, true);
                    return;
                }
                throw new Error(text);
            }
            form.querySelectorAll('.filled').forEach(i => i.classList.remove('filled'));
            if (isNew) {
                navigate(`#/tenants/${saved.id}`, `Сохранено: ${saved.title}`);
            } else {
                page.querySelector('h2').textContent = saved.title;
                showMessage(`Сохранено: ${saved.title}`, false);
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
                navigate('#/tenants', `Удалено: ${tenant.title}`);
            } catch (e) {
                showMessage(e.message);
            }
        });
    }
}

/** ГГГГ-ММ-ДД в ДД.ММ.ГГГГ. */
function ruDate(iso) {
    const [y, m, d] = String(iso).split('-');
    return d ? `${d}.${m}.${y}` : iso;
}

/** Таблица договоров (список и расчёт): номер ссылкой и флажок для печати, если договор уже создан. */
function contractsTable(list, selectable = false) {
    const rows = list.map(c => `<tr>
            ${selectable ? `<td><input type="checkbox" class="pick" value="${c.id}" aria-label="выбрать ${esc(c.number)}"></td>` : ''}
            <td>${c.id ? `<a href="#/contracts/${c.id}">${esc(c.number)}</a>` : esc(c.number)}</td>
            <td>${esc(c.tenant_title)}</td>
            <td>${ruDate(c.start)} – ${ruDate(c.end)}</td>
            <td class="num">${c.months}</td>
            <td class="num">${esc(c.rent)}</td>
            <td class="num">${esc(c.total)}</td>
        </tr>`).join('');
    return `<table class="list contracts">
        <thead><tr>${selectable ? '<th><input type="checkbox" id="pick-all" aria-label="выбрать все"></th>' : ''}<th>Номер</th><th>Арендатор</th>
            <th>Срок</th><th class="num">Мес.</th><th class="num">Плата в месяц</th><th class="num">За срок</th></tr></thead>
        <tbody>${rows}</tbody>
    </table>`;
}

/** Открывает печатную форму договоров в новой вкладке. */
function printContracts(ids) {
    window.open(ids.length === 1 ? `/print/contracts/${ids[0]}` : `/print/contracts?ids=${ids.join(',')}`, '_blank');
}

/** Адрес списка договоров с отбором: «← Договоры» в карточке возвращает к нему. */
let contractsListHash = '#/contracts';

/** id договоров, созданных последним документом: кнопка «Напечатать созданные» в списке. */
let lastCreated = [];

async function renderContracts(page, params) {
    contractsListHash = location.hash || '#/contracts';
    const filter = {tenant_id: params.get('tenant') || '', landlord_id: params.get('landlord') || '', from: params.get('from') || '', to: params.get('to') || ''};
    const query = new URLSearchParams(Object.entries(filter).filter(([, v]) => v !== '')).toString();
    const [list, tenants, landlords] = await Promise.all([
        api('GET', `/api/contracts${query ? `?${query}` : ''}`), api('GET', '/api/tenants'), api('GET', '/api/landlords'),
    ]);
    const filtered = Object.values(filter).some(v => v !== '');
    const created = lastCreated.filter(id => list.some(c => c.id === id));
    const option = (value, title, current) => `<option value="${value}"${String(value) === current ? ' selected' : ''}>${esc(title)}</option>`;
    page.innerHTML = `<h2>Договоры</h2>
        <p><a class="button" href="#/contracts/new">+ создать договоры</a>
            ${created.length === 0 ? '' : `<button type="button" id="print-created">Напечатать созданные (${created.length})</button>`}
            ${list.length === 0 ? '' : '<button type="button" id="print-picked" disabled>Печать выбранных</button>'}</p>
        <form class="filters" id="filters">
            <select name="tenant"><option value="">все арендаторы</option>${tenants.map(t => option(t.id, t.title, filter.tenant_id)).join('')}</select>
            <select name="landlord"><option value="">все арендодатели</option>${landlords.map(l => option(l.id, l.name_short, filter.landlord_id)).join('')}</select>
            <label>действует с <input type="date" name="from" value="${esc(filter.from)}"></label>
            <label>по <input type="date" name="to" value="${esc(filter.to)}"></label>
            ${filtered ? '<button type="button" id="reset">Сбросить</button>' : ''}
            <input id="filter" placeholder="поиск по номеру или арендатору" class="filter">
        </form>
        ${list.length === 0 ? `<p class="muted">${filtered ? 'По отбору договоров нет.' : 'Договоров пока нет.'}</p>` : contractsTable(list, true)}
        ${list.length === 0 ? '' : `<p class="muted">Договоров: ${list.length}</p>`}`;
    const filters = page.querySelector('#filters');
    filters.addEventListener('change', event => {
        if (event.target.id === 'filter') {
            return;
        }
        const next = new URLSearchParams([...new FormData(filters)].filter(([, v]) => v !== ''));
        location.hash = `#/contracts${next.toString() ? `?${next}` : ''}`;
    });
    const reset = page.querySelector('#reset');
    if (reset) {
        reset.addEventListener('click', () => { location.hash = '#/contracts'; });
    }
    if (created.length !== 0) {
        page.querySelector('#print-created').addEventListener('click', () => printContracts(created));
    }
    const search = page.querySelector('#filter');
    search.addEventListener('input', () => {
        const text = search.value.trim().toLowerCase();
        page.querySelectorAll('table.contracts tbody tr').forEach(tr => {
            tr.hidden = text !== '' && !tr.textContent.toLowerCase().includes(text);
        });
    });
    if (list.length === 0) {
        return;
    }
    const picks = () => [...page.querySelectorAll('input.pick')];
    const printPicked = page.querySelector('#print-picked');
    const updatePicked = () => {
        const n = picks().filter(i => i.checked).length;
        printPicked.disabled = n === 0;
        printPicked.textContent = n === 0 ? 'Печать выбранных' : `Печать выбранных (${n})`;
    };
    page.querySelector('table.contracts').addEventListener('change', event => {
        if (event.target.id === 'pick-all') {
            // только видимые после поиска
            picks().filter(i => !i.closest('tr').hidden).forEach(i => { i.checked = event.target.checked; });
        }
        updatePicked();
    });
    printPicked.addEventListener('click', () => printContracts(picks().filter(i => i.checked).map(i => Number(i.value))));
}

/** Строка документа «Создание договоров». */
function requestRow(tenants, row = {}) {
    const options = tenants.map(t => `<option value="${t.id}"${t.id === row.tenant_id ? ' selected' : ''}>${esc(t.title)}</option>`).join('');
    return `<tr>
        <td><select data-col="tenant_id"><option value="">выберите арендатора</option>${options}</select></td>
        <td><input data-col="start" type="date" value="${esc(row.start || '')}"></td>
        <td><input data-col="rent" value="${esc(row.rent || '')}" placeholder="20 000" class="num"></td>
        <td><button type="button" class="remove-row" title="Удалить строку">✕</button></td>
    </tr>`;
}

async function renderContractsNew(page) {
    const [landlords, tenants] = await Promise.all([api('GET', '/api/landlords'), api('GET', '/api/tenants')]);
    if (landlords.length === 0 || tenants.length === 0) {
        page.innerHTML = `<h2>Создание договоров</h2>
            <p>Сначала заведите ${landlords.length === 0 ? '<a href="#/landlords/new">арендодателя</a>' : ''}
            ${landlords.length === 0 && tenants.length === 0 ? 'и' : ''}
            ${tenants.length === 0 ? '<a href="#/tenants/new">арендатора</a>' : ''}.</p>`;
        return;
    }
    const today = new Date();
    const iso = `${today.getFullYear()}-${String(today.getMonth() + 1).padStart(2, '0')}-${String(today.getDate()).padStart(2, '0')}`;
    page.innerHTML = `<h2>Создание договоров</h2>
        <p><a href="#/contracts">← Договоры</a></p>
        <form class="card">
            <fieldset><legend>Арендодатель</legend>
                <label><span>Арендодатель *</span><select name="landlord_id">
                    ${landlords.map(l => `<option value="${l.id}">${esc(l.name_short)}</option>`).join('')}</select></label>
                <p class="muted" id="terms"></p>
            </fieldset>
            <fieldset><legend>Арендаторы</legend>
                <table class="equipment requests">
                    <thead><tr><th>Арендатор</th><th>Дата начала</th><th>Плата в месяц, руб.</th><th></th></tr></thead>
                    <tbody>${requestRow(tenants, {start: iso})}</tbody>
                </table>
                <p><button type="button" id="add-row">+ строка</button></p>
            </fieldset>
            <div class="actions">
                <button type="button" id="plan">Рассчитать</button>
                <button type="submit">Создать договоры</button>
            </div>
        </form>
        <div id="plan-result"></div>`;
    const form = page.querySelector('form');
    const table = form.querySelector('table.requests');
    const landlordSelect = form.querySelector('[name=landlord_id]');
    const showTerms = () => {
        const l = landlords.find(x => String(x.id) === landlordSelect.value);
        form.querySelector('#terms').textContent =
            `Срок аренды ${l.lease_months} мес. делится на договоры не больше ${l.contract_limit} руб. каждый (условия в карточке арендодателя).`;
    };
    landlordSelect.addEventListener('change', showTerms);
    showTerms();
    const body = () => ({
        landlord_id: Number(landlordSelect.value),
        rows: [...table.querySelectorAll('tbody tr')].map(tr => {
            const get = col => tr.querySelector(`[data-col=${col}]`).value;
            return {tenant_id: get('tenant_id') === '' ? null : Number(get('tenant_id')), start: get('start'), rent: get('rent')};
        }),
    });
    form.querySelector('#add-row').addEventListener('click', () => {
        const last = [...table.querySelectorAll('tbody tr [data-col=start]')].pop();
        table.querySelector('tbody').insertAdjacentHTML('beforeend', requestRow(tenants, {start: last ? last.value : iso}));
    });
    table.addEventListener('click', event => {
        if (event.target.closest('.remove-row')) {
            event.target.closest('tr').remove();
        }
    });
    const result = page.querySelector('#plan-result');
    form.querySelector('#plan').addEventListener('click', async () => {
        try {
            const plan = await api('POST', '/api/contracts/plan', body());
            showMessage('');
            result.innerHTML = `<h3>Будут созданы договоры: ${plan.length}</h3>${contractsTable(plan)}`;
        } catch (e) {
            result.innerHTML = '';
            showMessage(e.message);
        }
    });
    form.addEventListener('submit', async event => {
        event.preventDefault();
        try {
            const created = await api('POST', '/api/contracts', body());
            lastCreated = created.map(c => c.id);
            navigate('#/contracts', `Создано договоров: ${created.length}`);
        } catch (e) {
            showMessage(e.message);
        }
    });
}

/** Пара «подпись: значение» для карточки договора; пустое значение - прочерк. */
function infoRow(label, value) {
    return `<tr><th>${esc(label)}</th><td>${value ? esc(value) : '<span class="muted">не заполнено</span>'}</td></tr>`;
}

async function renderContract(page, idText) {
    const c = await api('GET', `/api/contracts/${idText}`);
    const t = c.tenant;
    const equipment = c.equipment.length === 0 ? '<p class="muted">Оборудование не используется.</p>' : `<table class="list">
        <thead><tr><th>Наименование</th><th>Марка, модель</th><th class="num">Мощность, кВт</th><th class="num">Кол-во</th></tr></thead>
        <tbody>${c.equipment.map(r => `<tr><td>${esc(r.name)}</td><td>${esc(r.model)}</td><td class="num">${esc(r.power)}</td><td class="num">${r.qty}</td></tr>`).join('')}</tbody>
        </table><p>Суммарная мощность: ${esc(c.total_power)} кВт</p>`;
    page.innerHTML = `<h2>Договор ${esc(c.number)}</h2>
        <p><a href="${esc(contractsListHash)}">← Договоры</a></p>
        <table class="info">
            ${infoRow('Срок', `${ruDate(c.start)} – ${ruDate(c.end)} (${c.months} мес.)`)}
            ${infoRow('Плата в месяц, руб.', c.rent)}
            ${infoRow('За срок, руб.', c.total)}
            ${infoRow('Арендодатель', c.landlord_title)}
            ${infoRow('Арендатор', `${c.tenant_title} (${c.kind_label}), ИНН ${t.inn}`)}
            ${infoRow('Площадь по плитке, кв. м', t.area)}
            ${infoRow('Площадь для торговли, кв. м', t.trade_area)}
            ${infoRow('Этаж', t.floor)}
            ${infoRow('Назначение', t.purpose)}
            ${infoRow('Вид деятельности', t.activity_gen)}
            ${infoRow('Состояние помещения', t.premises_condition)}
        </table>
        <h3>Оборудование (приложение 2)</h3>
        ${equipment}
        <p class="muted">Данные сторон скопированы в договор при создании. Если карточки исправлены, нажмите «Обновить из карточек».</p>
        <div class="actions">
            <button type="submit" id="print">Печать</button>
            <button type="button" id="refresh">Обновить из карточек</button>
            <button type="button" class="danger" id="remove">Удалить</button>
        </div>`;
    page.querySelector('#print').addEventListener('click', () => printContracts([c.id]));
    page.querySelector('#refresh').addEventListener('click', async () => {
        try {
            await api('POST', `/api/contracts/${idText}/refresh`);
            await renderContract(page, idText);
            showMessage('Данные сторон обновлены из карточек', false);
        } catch (e) {
            showMessage(e.message);
        }
    });
    page.querySelector('#remove').addEventListener('click', async () => {
        if (!confirm(`Удалить договор ${c.number}?`)) {
            return;
        }
        try {
            await api('DELETE', `/api/contracts/${idText}`);
            navigate(contractsListHash, `Удалён договор ${c.number}`);
        } catch (e) {
            showMessage(e.message);
        }
    });
}

async function render() {
    if (!currentUser) {
        return;
    }
    const [path, query] = (location.hash.replace(/^#\/?/, '') || MENU[0].path).split('?');
    document.getElementById('menu').innerHTML = MENU
        .filter(m => !m.admin || currentUser.role === 'admin')
        .map(m => `<a href="#/${m.path}"${path.split('/')[0] === m.path ? ' class="active"' : ''}>${m.title}</a>`)
        .join('');
    document.getElementById('account').innerHTML = `<span>${esc(currentUser.name || currentUser.login)}</span>
        <a href="#/password">Пароль</a> <a href="#" id="logout">Выйти</a>`;
    document.getElementById('logout').addEventListener('click', async event => {
        event.preventDefault();
        await api('POST', '/api/auth/logout');
        currentUser = null;
        history.replaceState(null, '', location.pathname);
        renderLogin();
    });
    const page = document.getElementById('page');
    const route = ROUTES.find(r => r.re.test(path));
    if (!route) {
        page.innerHTML = '<h2>Нет такой страницы</h2>';
        return;
    }
    try {
        await route.render(page, ...path.match(route.re).slice(1), new URLSearchParams(query || ''));
    } catch (e) {
        if (currentUser) {
            page.innerHTML = '<h2>Ошибка</h2>';
            showMessage(e.message);
        }
    }
}

/** Форма входа или создания первого администратора вместо приложения. */
function renderLogin(setup = false) {
    document.getElementById('menu').innerHTML = '';
    document.getElementById('account').innerHTML = '';
    const page = document.getElementById('page');
    page.innerHTML = `<h2>${setup ? 'Первый запуск: создайте администратора' : 'Вход'}</h2>
        <form class="card login">
            ${setup ? '<p class="muted">Администратор заводит остальных пользователей. Пароль не короче 6 символов.</p>' : ''}
            <label><span>Логин</span><input name="login" autocomplete="username" required></label>
            ${setup ? '<label><span>Имя</span><input name="name" placeholder="как показывать в программе"></label>' : ''}
            <label><span>Пароль</span><input name="password" type="password" autocomplete="${setup ? 'new-password' : 'current-password'}" required></label>
            ${setup ? '<label><span>Пароль ещё раз</span><input name="password2" type="password" autocomplete="new-password" required></label>' : ''}
            <div class="actions"><button type="submit">${setup ? 'Создать администратора' : 'Войти'}</button></div>
        </form>`;
    const form = page.querySelector('form');
    form.querySelector('[name=login]').focus();
    form.addEventListener('submit', async event => {
        event.preventDefault();
        const body = Object.fromEntries(new FormData(form));
        if (setup && body.password !== body.password2) {
            showMessage('пароли не совпадают');
            return;
        }
        delete body.password2;
        try {
            const {user} = await api('POST', setup ? '/api/auth/setup' : '/api/auth/login', body);
            currentUser = user;
            showMessage('');
            render();
        } catch (e) {
            showMessage(e.message);
        }
    });
}

async function renderUsers(page) {
    const users = await api('GET', '/api/users');
    const rows = users.map(u => `<tr data-id="${u.id}">
            <td>${esc(u.login)}</td><td>${esc(u.name)}</td>
            <td>${u.role === 'admin' ? 'администратор' : 'пользователь'}</td>
            <td>${u.blocked ? 'заблокирован' : 'действует'}</td>
            <td class="actions">
                <button type="button" data-act="role">${u.role === 'admin' ? 'Сделать пользователем' : 'Сделать администратором'}</button>
                <button type="button" data-act="block">${u.blocked ? 'Разблокировать' : 'Заблокировать'}</button>
                <button type="button" data-act="password">Новый пароль</button>
            </td>
        </tr>`).join('');
    page.innerHTML = `<h2>Пользователи</h2>
        <table class="list users"><thead><tr><th>Логин</th><th>Имя</th><th>Роль</th><th>Состояние</th><th></th></tr></thead>
        <tbody>${rows}</tbody></table>
        <h3>Новый пользователь</h3>
        <form class="card">
            <label><span>Логин *</span><input name="login" autocomplete="off" required></label>
            <label><span>Имя</span><input name="name"></label>
            <label><span>Пароль *</span><input name="password" type="password" autocomplete="new-password" required></label>
            <label><span>Роль</span><select name="role"><option value="user">пользователь</option><option value="admin">администратор</option></select></label>
            <div class="actions"><button type="submit">Добавить</button></div>
        </form>`;
    page.querySelector('form').addEventListener('submit', async event => {
        event.preventDefault();
        try {
            const user = await api('POST', '/api/users', Object.fromEntries(new FormData(event.target)));
            await renderUsers(page);
            showMessage(`Добавлен пользователь ${user.login}`, false);
        } catch (e) {
            showMessage(e.message);
        }
    });
    page.querySelector('table.users').addEventListener('click', async event => {
        const button = event.target.closest('button[data-act]');
        if (!button) {
            return;
        }
        const user = users.find(u => String(u.id) === button.closest('tr').dataset.id);
        let body;
        if (button.dataset.act === 'role') {
            body = {role: user.role === 'admin' ? 'user' : 'admin'};
        } else if (button.dataset.act === 'block') {
            body = {blocked: !user.blocked};
        } else {
            const password = prompt(`Новый пароль для ${user.login} (не короче 6 символов)`);
            if (!password) {
                return;
            }
            body = {password};
        }
        try {
            await api('PUT', `/api/users/${user.id}`, body);
            await renderUsers(page);
            showMessage(`Изменён пользователь ${user.login}`, false);
        } catch (e) {
            showMessage(e.message);
        }
    });
}

/** Размер файла для людей: «1,5 МБ». */
function fileSize(bytes) {
    const units = ['Б', 'КБ', 'МБ', 'ГБ'];
    let value = bytes;
    let unit = 0;
    while (value >= 1024 && unit < units.length - 1) {
        value /= 1024;
        unit++;
    }
    return `${value.toLocaleString('ru-RU', {maximumFractionDigits: 1})} ${units[unit]}`;
}

async function renderBackups(page, params = null, state = null) {
    const data = state || await api('GET', '/api/backups');
    const s = data.schedule;
    const rows = data.backups.map(b => `<tr>
            <td>${esc(b.created)}</td><td>${b.scheduled ? 'по расписанию' : 'вручную'}</td><td class="num">${fileSize(b.size)}</td>
            <td class="actions"><a class="button" href="/api/backups/${encodeURIComponent(b.name)}" download>Скачать</a>
                <button type="button" class="danger" data-name="${esc(b.name)}">Удалить</button></td>
        </tr>`).join('');
    page.innerHTML = `<h2>Резервные копии</h2>
        <p class="muted">Каталог копий: ${esc(data.dir)}. Восстановление из копии описано в README.</p>
        <p><button type="button" id="backup-now">Сделать копию сейчас</button></p>
        <form class="card" id="schedule">
            <fieldset><legend>Копии по расписанию</legend>
                <label><span>Делать каждый день</span><input type="checkbox" name="enabled"${s.enabled ? ' checked' : ''}></label>
                <label><span>Время</span><input type="time" name="time" value="${esc(s.time)}"></label>
                <label><span>Хранить последних копий</span><input type="number" name="keep" min="1" max="365" value="${s.keep}"></label>
                <p class="muted">Если в это время сервер выключен, копия сделается после включения в тот же день.
                    Ручные копии не удаляются.</p>
                ${s.last_run ? `<p>Последняя копия по расписанию: ${ruDate(s.last_run)}${s.last_error ? ` <span class="error">ошибка: ${esc(s.last_error)}</span>` : ''}</p>` : ''}
            </fieldset>
            <div class="actions"><button type="submit">Сохранить расписание</button></div>
        </form>
        ${data.backups.length === 0 ? '<p class="muted">Копий пока нет.</p>' : `<table class="list backups">
            <thead><tr><th>Сделана</th><th>Как</th><th class="num">Размер</th><th></th></tr></thead><tbody>${rows}</tbody></table>`}`;
    page.querySelector('#backup-now').addEventListener('click', async () => {
        try {
            const next = await api('POST', '/api/backups');
            await renderBackups(page, null, next);
            showMessage(`Сделана копия ${next.created}`, false);
        } catch (e) {
            showMessage(e.message);
        }
    });
    page.querySelector('#schedule').addEventListener('submit', async event => {
        event.preventDefault();
        const form = event.target;
        try {
            const next = await api('PUT', '/api/backups/schedule', {
                enabled: form.enabled.checked, time: form.time.value, keep: Number(form.keep.value),
            });
            await renderBackups(page, null, next);
            showMessage('Расписание сохранено', false);
        } catch (e) {
            showMessage(e.message);
        }
    });
    page.querySelectorAll('button[data-name]').forEach(button => button.addEventListener('click', async () => {
        if (!confirm(`Удалить копию ${button.dataset.name}?`)) {
            return;
        }
        try {
            await renderBackups(page, null, await api('DELETE', `/api/backups/${encodeURIComponent(button.dataset.name)}`));
            showMessage('Копия удалена', false);
        } catch (e) {
            showMessage(e.message);
        }
    }));
}

async function renderPassword(page) {
    page.innerHTML = `<h2>Смена пароля</h2>
        <form class="card">
            <label><span>Текущий пароль</span><input name="old_password" type="password" autocomplete="current-password" required></label>
            <label><span>Новый пароль</span><input name="password" type="password" autocomplete="new-password" required></label>
            <label><span>Новый пароль ещё раз</span><input name="password2" type="password" autocomplete="new-password" required></label>
            <div class="actions"><button type="submit">Сменить пароль</button></div>
        </form>`;
    page.querySelector('form').addEventListener('submit', async event => {
        event.preventDefault();
        const body = Object.fromEntries(new FormData(event.target));
        if (body.password !== body.password2) {
            showMessage('пароли не совпадают');
            return;
        }
        try {
            await api('PUT', '/api/auth/password', {old_password: body.old_password, password: body.password});
            event.target.reset();
            showMessage('Пароль изменён. На других устройствах нужно войти заново.', false);
        } catch (e) {
            showMessage(e.message);
        }
    });
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

/** Сообщение для страницы, на которую переходим: показывается после смены адреса. */
let pendingMessage = null;

/** Переход на адрес с сообщением на новой странице (по умолчанию об успехе). */
function navigate(hash, text, isError = false) {
    pendingMessage = {text, isError};
    location.hash = hash;
}

window.addEventListener('hashchange', () => {
    showMessage(pendingMessage ? pendingMessage.text : '', pendingMessage ? pendingMessage.isError : true);
    pendingMessage = null;
    render();
});
/** Запуск: первый администратор, вход или приложение. */
async function start() {
    try {
        const auth = await api('GET', '/api/auth');
        currentUser = auth.user;
        if (auth.setup) {
            renderLogin(true);
        } else if (!currentUser) {
            renderLogin();
        } else {
            render();
        }
    } catch (e) {
        showMessage(e.message);
    }
}

start();
showVersion();
