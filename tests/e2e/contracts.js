// Договоры: стороны заводятся через API, документ «Создание договоров» с двумя строками,
// расчёт без записи, создание, список с поиском, карточка, обновление из карточек и удаление.
const landlord = await api('POST', '/api/landlords', {name_full: 'ООО «Ромашка»', name_short: 'ООО «Ромашка»'});
const ip = await api('POST', '/api/tenants', {kind: 'ip', name: 'Петров Пётр Петрович', gender: 'm', inn: '500100732259', floor: '2'});
const org = await api('POST', '/api/tenants', {kind: 'org', name_full: 'ООО «Берёза»', name_short: 'ООО «Берёза»', inn: '7707083893'});
location.hash = '#/contracts/new';
await sleep(600);
expect(document.getElementById('terms').textContent.includes('11 мес.'), 'нет условий арендодателя: ' + document.getElementById('terms').textContent);

const rows = () => [...document.querySelectorAll('table.requests tbody tr')];
const set = (tr, col, value) => { tr.querySelector(`[data-col=${col}]`).value = value; };
set(rows()[0], 'tenant_id', String(ip.id));
set(rows()[0], 'start', '2026-10-19');
set(rows()[0], 'rent', '20 000');
btn('+ строка').click();
expect(rows()[1].querySelector('[data-col=start]').value === '2026-10-19', 'новая строка не взяла дату предыдущей');
set(rows()[1], 'tenant_id', String(org.id));
set(rows()[1], 'start', '2026-11-01');

btn('Рассчитать').click();
await sleep(500);
expect(document.getElementById('message').textContent.includes('строка 2 (ООО «Берёза»): плата в месяц: не заполнено'),
    'нет ошибки пустой платы: ' + document.getElementById('message').textContent);
set(rows()[1], 'rent', '9000');
btn('Рассчитать').click();
await sleep(500);
const planned = [...document.querySelectorAll('#plan-result tbody tr')].map(tr => tr.cells[0].textContent + ' ' + tr.cells[3].textContent);
log.push('расчёт: ' + planned.join('; '));
expect(planned.join('; ') === '2026-10-19-500100732259 4; 2027-02-19-500100732259 4; 2027-06-19-500100732259 3; 2026-11-01-7707083893 11',
    'расчёт: ' + planned.join('; '));
expect((await api('GET', '/api/contracts')).length === 0, 'расчёт записал договоры');

btn('Создать договоры').click();
await sleep(700);
expect(location.hash === '#/contracts', 'после создания не открылся список: ' + location.hash);
expect(document.querySelectorAll('table.contracts tbody tr').length === 4, 'в списке не 4 договора');
const filter = document.getElementById('filter');
filter.value = 'берёза';
filter.dispatchEvent(new Event('input'));
const visible = [...document.querySelectorAll('table.contracts tbody tr')].filter(tr => !tr.hidden);
expect(visible.length === 1, 'поиск «берёза» показал строк: ' + visible.length);

visible[0].querySelector('a').click();
await sleep(600);
expect(document.querySelector('h2').textContent === 'Договор 2026-11-01-7707083893', 'заголовок: ' + document.querySelector('h2').textContent);
await api('PUT', `/api/tenants/${org.id}`, {floor: '3'});
btn('Обновить из карточек').click();
await sleep(600);
const floorRow = [...document.querySelectorAll('table.info tr')].find(tr => tr.cells[0].textContent === 'Этаж');
expect(floorRow && floorRow.cells[1].textContent === '3', 'этаж после обновления: ' + (floorRow && floorRow.cells[1].textContent));

btn('Удалить').click();
await sleep(600);
expect(location.hash === '#/contracts', 'после удаления не вернулись к списку');
expect(document.querySelectorAll('table.contracts tbody tr').length === 3, 'договор не удалён');
log.push('создано 4, удалён 1');
