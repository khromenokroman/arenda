// Виды деятельности: заведение в справочнике, отказ на повтор названия, выбор в карточке арендатора
// подставляет тексты договора, и они сохраняются в карточке; удаление вида карточку не меняет.
const input = name => document.querySelector(`form.card [name=${name}]`);
const message = () => document.getElementById('message');
const pick = () => document.getElementById('activity-pick');

location.hash = '#/activities/new';
await sleep(400);
input('name').value = 'Металлоремонт';
input('purpose').value = 'оказания услуг по металлоремонту';
input('activity_gen').value = 'металлоремонта';
input('premises_condition').value = 'удовлетворительное';
btn('Сохранить').click();
await sleep(500);
expect(location.hash === '#/activities', 'после сохранения не открылся список: ' + location.hash);
expect(document.querySelector('table.list').textContent.includes('оказания услуг по металлоремонту'), 'нет в списке');

location.hash = '#/activities/new';
await sleep(400);
input('name').value = 'Металлоремонт';
btn('Сохранить').click();
await sleep(500);
expect(message().className === 'error' && message().textContent.includes('уже есть'), 'повтор названия: ' + message().textContent);
log.push('повтор: ' + message().textContent);

location.hash = '#/tenants/new';
await sleep(500);
expect(pick(), 'в карточке арендатора нет выбора вида деятельности');
pick().value = pick().options[1].value;
pick().dispatchEvent(new Event('change'));
expect(input('purpose').value === 'оказания услуг по металлоремонту', 'назначение: ' + input('purpose').value);
expect(input('activity_gen').value === 'металлоремонта', 'вид деятельности: ' + input('activity_gen').value);
expect(input('premises_condition').value === 'удовлетворительное', 'состояние: ' + input('premises_condition').value);
expect(input('purpose').classList.contains('filled'), 'подставленное поле не подсвечено');
log.push(message().textContent);

input('kind').value = 'person';
input('kind').dispatchEvent(new Event('change'));
await sleep(100);
expect(pick() && input('purpose').value === 'оказания услуг по металлоремонту', 'тексты потерялись при смене вида');
input('name').value = 'Сидорова Анна Сергеевна';
input('gender').value = 'f';
input('inn').value = '526317984689';
input('activity_gen').value = 'ремонта обуви и металлоремонта';
btn('Сохранить').click();
await sleep(700);
expect(/^#\/tenants\/\d+$/.test(location.hash), 'арендатор не сохранился: ' + location.hash + ' ' + message().textContent);
const tenant = location.hash;
expect(input('activity_gen').value === 'ремонта обуви и металлоремонта', 'поправленный текст: ' + input('activity_gen').value);

location.hash = '#/activities';
await sleep(400);
document.querySelector('table.list a').click();
await sleep(400);
btn('Удалить').click();
await sleep(500);
expect(document.querySelector('table.list') === null, 'вид деятельности не удалён');
location.hash = tenant;
await sleep(500);
expect(input('purpose').value === 'оказания услуг по металлоремонту', 'удаление вида изменило карточку: ' + input('purpose').value);
expect(pick() === null && document.querySelector('#kind-fields a[href="#/activities"]'), 'при пустом справочнике нет ссылки на него');
