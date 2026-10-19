// Резервные копии: копия вручную, скачивание, расписание, удаление.
const message = () => document.getElementById('message');
expect(document.querySelector('h2').textContent === 'Резервные копии', 'нет страницы копий');
btn('Сделать копию сейчас').click();
await sleep(800);
expect(message().textContent.startsWith('Сделана копия arenda-'), 'копия не сделана: ' + message().textContent);
const rows = () => [...document.querySelectorAll('table.backups tbody tr')];
expect(rows().length === 1 && rows()[0].cells[1].textContent === 'вручную', 'в списке нет ручной копии');
const link = rows()[0].querySelector('a');
const file = await (await fetch(link.href)).arrayBuffer();
expect(new TextDecoder().decode(file.slice(0, 15)) === 'SQLite format 3', 'по ссылке не база SQLite');

const form = document.getElementById('schedule');
form.time.value = '21:30';
form.keep.value = '5';
btn('Сохранить расписание').click();
await sleep(600);
expect(message().textContent === 'Расписание сохранено', 'расписание: ' + message().textContent);
const saved = await api('GET', '/api/backups');
expect(saved.schedule.time === '21:30' && saved.schedule.keep === 5, 'расписание не сохранилось: ' + JSON.stringify(saved.schedule));

rows()[0].querySelector('button.danger').click();
await sleep(600);
expect(rows().length === 0, 'копия не удалена');
log.push('копия, скачивание, расписание, удаление');
