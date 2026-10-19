// Арендаторы: смена вида перестраивает форму и сохраняет общие поля, ошибка даты показывается,
// физлицо сохраняется с паспортом, в списке видны название и вид.
const input = name => document.querySelector(`form.card [name=${name}]`);
const legends = () => [...document.querySelectorAll('#kind-fields legend')].map(l => l.textContent).join(', ');
const message = () => document.getElementById('message');

expect(input('kind').value === 'ip', 'по умолчанию не ИП: ' + input('kind').value);
expect(input('ogrnip') && !input('passport'), 'у ИП нет поля ОГРНИП или есть паспорт');
expect(input('gender').tagName === 'SELECT', 'пол не выбор из списка');
input('name').value = 'Сидорова Анна Сергеевна';
input('gender').value = 'f';

input('kind').value = 'person';
input('kind').dispatchEvent(new Event('change'));
await sleep(100);
log.push('разделы физлица: ' + legends());
expect(legends() === 'Основное, Паспорт, Банк', 'разделы физлица: ' + legends());
expect(input('name').value === 'Сидорова Анна Сергеевна', 'ФИО потерялось при смене вида');
expect(input('gender').value === 'f', 'пол потерялся при смене вида');
expect(!input('ogrnip'), 'у физлица осталось поле ОГРНИП');
expect(input('birth_date').type === 'date', 'дата рождения не поле даты');

input('passport').value = '4510123456';
btn('Сохранить').click();
await sleep(500);
expect(message().className === 'error' && message().textContent.includes('как «1234 567890»'), 'нет ошибки паспорта: ' + message().textContent);
log.push('ошибка паспорта: ' + message().textContent);

input('passport').value = '4510 123456';
input('birth_date').value = '1990-05-17';
input('passport_code').value = '770-001';
btn('Сохранить').click();
await sleep(600);
expect(/^#\/tenants\/\d+$/.test(location.hash), 'после сохранения не открылась карточка: ' + location.hash);
expect(input('kind').value === 'person', 'вид после сохранения: ' + input('kind').value);
expect(input('birth_date').value === '1990-05-17', 'дата рождения: ' + input('birth_date').value);
expect(input('passport').value === '4510 123456', 'паспорт: ' + input('passport').value);

location.hash = '#/tenants';
await sleep(500);
const rows = [...document.querySelectorAll('table.list tbody tr')].map(r => r.cells[0].textContent + ' / ' + r.cells[1].textContent);
log.push('список: ' + rows.join('; '));
expect(rows.join('; ') === 'Сидорова Анна Сергеевна / Физическое лицо', 'в списке: ' + rows.join('; '));
