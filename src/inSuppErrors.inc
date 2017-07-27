<?php

namespace AveWd;

use AveWd\Console\Command\Command;
use Symfony\Component\Console\Helper\FormatterHelper;
use Symfony\Component\Console\Input\InputDefinition;
use Symfony\Component\Console\Input\InputInterface;
use Symfony\Component\Console\Output\OutputInterface;
use Symfony\Component\Console\Input\InputOption;

class inSuppErrors extends Command
{
    public function __construct(string $name = null)
    {
        parent::__construct($name);
    }

    protected function configure()
    {
        parent::configure();
        $this->setDescription('Проверка не отправленных на аптеку приходов ( кроме интернет-заказов )');
        $this->setHelp('Здесь будет описание комманды' . PHP_EOL . PHP_EOL);
        $this->setDefinition(new InputDefinition([
            new InputOption('email', 'e', InputOption::VALUE_IS_ARRAY | InputOption::VALUE_REQUIRED,
                'Адрес электронной почты для отправки результата'),
            new InputOption('only-work-time', 'w', InputOption::VALUE_NONE,
                'Работать только в рабочее время с 9-18 с понедельника по пятницу'),
                ]));
    }
    protected function initialize(InputInterface $input, OutputInterface $output)
    {
        parent::initialize($input,$output);
    }
    protected function interact(InputInterface $input, OutputInterface $output)
    {
        parent::interact($input,$output);
    }
    protected function execute(InputInterface $input, OutputInterface $output)
    {
        /** @var FormatterHelper $formatter */
        $formatter = &$this->formatter;
        parent::execute($input, $output);
        if($input->getOption('only-work-time'))
        if(\Carbon\Carbon::now()->isWeekday())
        if(\Carbon\Carbon::now()->hour<9 || \Carbon\Carbon::now()->hour>18){
            $output->writeln($formatter->formatBlock('Не работаем в нерабочее время','info',true));
            die;
        }

        $output->writeln('Готовим PDO');
        $this->_pg__ave_skd = new \PDO($this->config->database->ave_skd->dsn, $this->config->database->ave_skd->username, $this->config->database->ave_skd->password);
        $this->_pg__repl = new \PDO($this->config->database->transport->dsn, $this->config->database->transport->username, $this->config->database->transport->password);

        $output->writeln('Настраиваем драйвер БД');
        // Генерировать исключение при ошибках
        $this->_pg__ave_skd->setAttribute(\PDO::ATTR_ERRMODE, \PDO::ERRMODE_EXCEPTION);
        $this->_pg__repl->setAttribute(\PDO::ATTR_ERRMODE, \PDO::ERRMODE_EXCEPTION);
        // Преобразовать пустые строки в NULL
        $this->_pg__ave_skd->setAttribute(\PDO::ATTR_ORACLE_NULLS, \PDO::NULL_EMPTY_STRING);
        $this->_pg__repl->setAttribute(\PDO::ATTR_ORACLE_NULLS, \PDO::NULL_EMPTY_STRING);
        // Преобразовывать числовые значения в строки во время выборки. Значение типа bool.
        $this->_pg__ave_skd->setAttribute(\PDO::ATTR_STRINGIFY_FETCHES, false);
        $this->_pg__repl->setAttribute(\PDO::ATTR_STRINGIFY_FETCHES, false);
        // Режим выборки по умолчанию
        $this->_pg__ave_skd->setAttribute(\PDO::ATTR_DEFAULT_FETCH_MODE, \PDO::FETCH_NAMED);
        $this->_pg__repl->setAttribute(\PDO::ATTR_DEFAULT_FETCH_MODE, \PDO::FETCH_NAMED);


        // Запрос в 1С, выбираем плохие интенет-заказы
        $output->write('Запрос в 1С, выбираем плохие интенет-заказы:');
        try {
            $this->_pg__ave_skd = $this->_pg__ave_skd->query(file_get_contents(__DIR__ . '/../sql/pg/getBadSuppErrors.sql'));
        } catch (PDOException  $e) {
            die("Ошибка: " . $e);
        }
        $output->writeln(
            $formatter->formatBlock(
                $this->_pg__ave_skd->rowCount() . ' стр.','num'));

        // Заверщаем работу если документов нет
        if (!$this->_pg__ave_skd->rowCount()) {
        $output->writeln(
            $formatter->formatBlock(
                'Нет плохих приходов','green'));
            exit(0);
        }


        // Приводим в нормальный вид выгрузку из 1С

        $output->writeln('Приводим в нормальный вид выгрузку из 1С');
        $this->_pg__ave_skd->setFetchMode(\PDO::FETCH_ASSOC);
        $this->_pg__ave_skd = new \ArrayObject($this->_pg__ave_skd->fetchAll(), \ArrayObject::ARRAY_AS_PROPS | \ArrayObject::STD_PROP_LIST);
        $_pg__ave_skd__iterator = $this->_pg__ave_skd->getIterator();
        while ($_pg__ave_skd__iterator->valid()) {
            $this->_ndocs[] = (int)$_pg__ave_skd__iterator->current()['ndoc'];
            $this->_idapts[] = (int)$_pg__ave_skd__iterator->current()['idapt'];
            $this->_pg__ave_skd->{$_pg__ave_skd__iterator->key()}['ddoc'] =
                \Carbon\Carbon::create(
                    $_pg__ave_skd__iterator->current()['ddoc_year'],
                    $_pg__ave_skd__iterator->current()['ddoc_month'],
                    $_pg__ave_skd__iterator->current()['ddoc_day'],
                    $_pg__ave_skd__iterator->current()['ddoc_hour'],
                    $_pg__ave_skd__iterator->current()['ddoc_minute'],
                    0, 'Europe/Moscow');
            // Определяем минимальныю дату документа
            if ((int)$_pg__ave_skd__iterator->current()['mddoc_year'] &&
                (int)$_pg__ave_skd__iterator->current()['mddoc_month'] &&
                (int)$_pg__ave_skd__iterator->current()['mddoc_day']
            ) $this->_mddoc = \Carbon\Carbon::createFromDate(
                $_pg__ave_skd__iterator->current()['mddoc_year'],
                $_pg__ave_skd__iterator->current()['mddoc_month'],
                $_pg__ave_skd__iterator->current()['mddoc_day']);
            $_pg__ave_skd__iterator->next();
        }


        // Готовим запрос в базу репликации
        $output->writeln('Готовим запрос в базу репликации');
        try {
            $sql = (new \Twig_Environment(new \Twig_Loader_Filesystem(realpath(__DIR__ . '/../sql/pg/twig')), [
                'cache' => realpath(__DIR__ . '/../cache/'),
                'auto_reload' => true,
                'optimizations' => (int)false,
            ]))->render('getTrasportErrors.sql.twig',
                [
                    'ndocs' => $this->_ndocs,
                    'mddoc' => $this->_mddoc,
                    'idapts' => array_unique($this->_idapts, SORT_NUMERIC),
                ]
            );
        } catch (\Twig_Error_Syntax $e) {

            $output->writeln('<err>' . $e->getSourceContext()->getPath() . '</err>');
            $output->writeln('<info>Строка #' . $e->getTemplateLine() . ': ' . $e->getRawMessage() . '</info>');
        }


        myZIP::$zPath = __DIR__ . '/../tmp/sql.'.\Carbon\Carbon::now()->timestamp.'.zip';
        myZIP::$fPath = __DIR__ . '/../tmp/getInternetOrdersTrasportErrors.sql';
        myZIP::$password = strtoupper(dechex(crc32($sql)));

        file_put_contents(myZIP::$fPath,$sql);myZIP::doZip();


        // Делаем запрос в базу репликации
        $output->write('Делаем запрос в базу репликации:');
        try {
            $this->_pg__repl = $this->_pg__repl->query($sql);
        } catch
        (\Exception $e) {
            die("Ошибка: " . $e);
        }
        $output->writeln($formatter->formatBlock($this->_pg__repl->rowCount(), 'num'));


        $this->_pg__repl->setFetchMode(\PDO::FETCH_ASSOC);
        $this->_pg__repl = new \ArrayObject($this->_pg__repl->fetchAll(), \ArrayObject::ARRAY_AS_PROPS | \ArrayObject::STD_PROP_LIST);
        $_pg__repl__iterator = $this->_pg__repl->getIterator();
        while ($_pg__repl__iterator->valid()) {
            $this->_pg__repl->{$_pg__repl__iterator->key()}['data'] = new \Carbon\Carbon($_pg__repl__iterator->current()['data']);


            // Обработка описания событий
            if ($this->_pg__repl->{$_pg__repl__iterator->key()}['msg'])
                $this->_pg__repl->{$_pg__repl__iterator->key()}['msg'] =
                    mb_convert_encoding($_pg__repl__iterator->current()['msg'], 'utf-8', 'Windows-1251');
            elseif ($_pg__repl__iterator->current()['tname'] == 'out')
                $this->_pg__repl->{$_pg__repl__iterator->key()}['msg'] =
                    'пакет поставлен в очередь на доставку в аптеку, если аптека его долго не забирает, видимо там проблемы со связью';


            $_pg__repl__iterator->next();
        }
        if (!$_pg__repl__iterator->count()) die(0);

        if ($input->getOption('email')) {
            $mail = new \PHPMailer;

            $mail->setLanguage('ru');
            $mail->CharSet = 'utf-8';

            $mail->setFrom('transport@monitoring.id-soft', 'Монитор транспорта накладных');
            $emails = (new \ArrayObject($input->getOption('email')))->getIterator();
            $mail->clearReplyTos();
            while ($emails->valid()) {
                $mail->addAddress($emails->current());
                $mail->addReplyTo($emails->current());
                $emails->next();
            }
            unset($emails);

            $mail->isHTML(true);
            $mail->Subject = 'Мониторинг приходов';
            if (file_exists(myZIP::$zPath)) $mail->addAttachment(myZIP::$zPath);
            $mail->addEmbeddedImage(__DIR__ . '/../img/filter-nointernet-orders-img.jpg', 'filter-nointernet-orders-img', 'filter-nointernet-orders-img.jpg');

            $mail->msgHTML((new \Twig_Environment(new \Twig_Loader_Filesystem(realpath(__DIR__ . '/../templates/'))))
                ->render('mail_bad_normal_orders.twig', ['docs' => $_pg__repl__iterator]));
            if (!$mail->send()) {
                $output->writeln(
                    $formatter->formatBlock('Ошибка отправки: '.$mail->ErrorInfo,'err',true));
            } else {
                $output->writeln(
                    $formatter->formatBlock('Message has been sent','green',true));
            }
        }
    }
}

$app->add(new inSuppErrors('check-normal-orders'));