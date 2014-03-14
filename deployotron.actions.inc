<?php
/**
 * @file
 * Actions for Deployotron!
 */

namespace Deployotron {
  class ActionFactory {
    static protected $actionMapping = array(
      'deploy' => array(
        'SanityCheck',
        'SiteOffline',
        'BackupDatabase',
        'DeployCode',
        'CreateVersionTxt',
        'RestartApache2',
        'UpdateDatabase',
        'RunDrakePostUpdate',
        'ClearCache',
        'SiteOnline',
        'RestartVarnish',
        'FlowdockNotificaton',
        'NewRelicNotificaton',
      ),
      'omg' => array(
        'OMGPrepare',
        'DeployCode',
        'RestoreDatabase',
      ),
    );

    /**
     * Gathers an array of actions to run.
     */
    static public function getActions($name, $site = NULL) {
      $actions = array();
      if (isset(static::$actionMapping[$name])) {
        foreach (static::$actionMapping[$name] as $class_name) {
          $class_name = '\\Deployotron\\Actions\\' . $class_name;
          $action = new $class_name($site);
          $actions[] = $action;
        }
        return $actions;
      }
    }

    /**
     * Output help on actions and options.
     */
    static public function getHelp() {
      $all_actions = array();
      drush_print('Commands');
      drush_print('--------');
      foreach (static::$actionMapping as $name => $actions) {
        drush_print(wordwrap(dt("@name runs the actions: @actions", array('@name' => $name, '@actions' => implode(', ', $actions)))));
        drush_print();

        $all_actions = array_merge($all_actions, $actions);
      }
      $all_actions = array_unique($all_actions);
      sort($all_actions);

      drush_print('Actions');
      drush_print('-------');
      foreach ($all_actions as $class_name) {
        drush_print($class_name . ':');
        $class_name = '\\Deployotron\\Actions\\' . $class_name;
        drush_print($class_name::getHelp());
        drush_print();

        // Print options.
        $options = $class_name::getOptions();
        if ($options) {
          drush_print_table(drush_format_help_section($options, 'options'));
          drush_print();
        }
        drush_print();
      }

    }
  }

  /**
   * Base class for actions.
   */
  abstract class Action {
    /**
     * Set a default short description.
     */
    static protected $short = "incompletely implemented";

    /**
     * Default run message.
     */
    static protected $runMessage = "Running action.";

    /**
     * Options for this action.
     */
    static protected $options = array();

    /**
     * The site this action works on.
     */
    protected $site;

    /**
     * Result object of last drush command.
     */
    protected $drushResult;

    /**
     * Array of the output of the last executed command.
     */
    protected $shOutput;

    /**
     * Return code of the last executed command.
     */
    protected $shRc;

    /**
     * Shas of checkouts of aliases.
     */
    static protected $headShas = array();

    public static function getHelp() {
      if (isset(static::$help)) {
        return static::$help;
      }
      if (isset(static::$description)) {
        return static::$description;
      }
      return "No description.";
    }

    /**
     * Get the command line options for this action.
     *
     * If the command has an enable-switch, the kill-switch and command options
     * are classed as sub-options for nice help output. If there's no
     * enable-switch, all options are considered regular options.
     *
     * @return array
     *   Options and sub-options as defined on commands in hook_drush_command().
     */
    public static function getOptions() {
      $options = array();
      if (isset(static::$killSwitch)) {
        $options += array(
          static::$killSwitch => "Don't " . static::$short . (isset(static::$enableSwitch) ? ' (overrides --' . static::$enableSwitch . ')' : '') . '.',
        );
      }

      $options += static::$options;

      if (isset(static::$enableSwitch)) {
        $sub_options = $options;
        $options = array();
        // If the action options defines the enable switch, use it so
        // description and possible examle-values propergate.
        if (isset($sub_options[static::$enableSwitch])) {
          $options[static::$enableSwitch] = $sub_options[static::$enableSwitch];
          unset($sub_options[static::$enableSwitch]);
        }
        else {
          $options[static::$enableSwitch] = ucfirst(static::$short) . '.';
        }
        return array(
          'options' => $options,
          'sub-options' => !empty($sub_options) ? array(static::$enableSwitch => $sub_options) : array(),
        );
      }

      return array('options' => $options, 'sub-options' => array());
    }

    /**
     * Construct a new Action object.
     */
    public function __construct($site) {
      $this->site = $site;

      // Note the devious use of static/self.
      if (static::$short == self::$short) {
        drush_log(dt('Incomplete action, missing short description: @action', array('@action' => get_class($this))), 'warning');
      }

      self::$runMessage = 'Running ' . static::$short;
    }

    /**
     * Get the short description of this action.
     *
     * If the action has a killswitch, it should work with "Don't " prepended.
     *
     * @return string
     *   Human readable short description of action taken.
     */
    public function getShort() {
      return static::$short;
    }

    /**
     * Return the message to print when running this action.
     *
     * @return string
     *   The Message (by Grandmaster Flash).
     */
    public function getRunMessage() {
      return static::$runMessage;
    }

    /**
     * Get a description of what the action would do.
     *
     * If the action subclass defines the $description property, use that, else
     * returns a generic message.
     *
     * If more logic than a static string is needed, subclassesare encouraged to
     * override this to provide a more specific description of the effect of the
     * action.
     *
     * Used when the user provides the --confirm switch.
     *
     * @return string
     *   Description of the action.
     */
    public function getDescription() {
      if (isset(static::$description)) {
        return static::$description;
      }
      return dt("Run the @short action.", array('@short' => static::$short));
    }

    /**
     * Validate that the action can be run.
     *
     * @return bool
     *   Whether it can.
     */
    public function validate() {
      return TRUE;
    }

    /**
     * Run the task.
     *
     * @param array $state
     *   Persistent state array that actions can use to communicate with
     *   following actions.
     *
     * @return bool
     *   Success or failure.
     */
    public function run($state) {
      return FALSE;
    }

    /**
     * Roll back the task.
     */
    public function rollback() {
      return 'no rollback';
    }

    /**
     * Whether this action is enabled.
     */
    public function enabled() {
      // Incomplete actions are always disabled.
      if (static::$short == self::$short) {
        return FALSE;
      }
      // Disable if there is a killswitch and it is set.
      if (isset(static::$killSwitch) && drush_get_option(static::$killSwitch, FALSE)) {
        return FALSE;
      }
      // If there is an enable switch, let that decide.
      if (isset(static::$enableSwitch)) {
        return drush_get_option(static::$enableSwitch, FALSE);
      }
      // Else default to enabled.
      return TRUE;
    }

    /**
     * Execute a sh command.
     *
     * Is run on the site.
     *
     * @param string $command
     *   Command to run.
     *
     * @return bool
     *   Whether the command succeeded.
     */
    protected function sh($command) {
      $exec = $command;
      // Check that there is a remote host before trying to construct a remote
      // command.
      $host = drush_remote_host($this->site);
      if (!empty($host)) {
        $exec = drush_shell_proc_build($this->site, $command, TRUE);
      }
      else {
        // Else just cd to the root of the alias. This allows us to test the
        // code without a remote host.
        $exec = "cd " . drush_escapeshellarg($this->site['root']) . " && " . $exec;
      }
      return $this->shLocal($exec);
    }

    /**
     * Get the output of the most recent command.
     *
     * @return string
     *   The output.
     */
    protected function shOutput() {
      return implode("\n", $this->shOutputArray());
    }

    /**
     * Get the output of the most recent command as an array.
     *
     * @return array
     *   Lines of output.
     */
    protected function shOutputArray() {
      return $this->shOutput;
    }

    /**
     * Get the return code of the most recent command.
     */
    protected function shRc() {
      return $this->shRc;
    }

    /**
     * Execute a sh locally.
     *
     * Works much like drush_shell_exec, however it captures the command return
     * code too, and doesn't support interactive mode.
     *
     * @param string $command
     *   Command to run.
     *
     * @return bool
     *   Whether the command succeeded.
     */
    protected function shLocal($command) {
      $this->shOutput = array();
      $this->shRc = 0;
      $args = func_get_args();
      // Escape args, but not the command.
      for ($x = 1; $x < count($args); $x++) {
        $args[$x] = drush_escapeshellarg($args[$x]);
      }
      // Mimic drush_shell_exec(), which can take a single or multiple args.
      if (count($args) == 1) {
        $command = $args[0];
      }
      else {
        $command = call_user_func_array('sprintf', $args);
      }

      if (drush_get_context('DRUSH_VERBOSE') || drush_get_context('DRUSH_SIMULATE')) {
        drush_print('Executing: ' . $command, 0, STDERR);
      }
      if (!drush_get_context('DRUSH_SIMULATE')) {
        exec($command . ' 2>&1', $this->shOutput, $this->shRc);

        if (drush_get_context('DRUSH_DEBUG')) {
          foreach ($this->shOutput as $line) {
            drush_print($line, 2);
          }
        }
      }

      // Exit code 0 means success.
      return ($this->shRc == 0);
    }

    /**
     * Execute a drush command.
     *
     * Is run on the site.
     *
     * @param string $command
     *   Command to run.
     * @param string $args
     *   Command arguments.
     * @param string $options
     *   Command arguments.
     *
     * @return bool
     *   Whether the command succeeded.
     */
    protected function drush($command, $args = array(), $options = array()) {
      $this->drushResult = drush_invoke_process($this->site, $command, $args, $options, TRUE);
      if (!$this->drushResult || $this->drushResult['error_status'] != 0) {
        return FALSE;
      }
      return TRUE;
    }

    /**
     * Get the SHA to deploy.
     *
     * This is based on options from configuration or command.
     *
     * @return string The SHA to deploy.
     */
    protected function pickSha($options = array()) {
      $options += array(
        'branch' => NULL,
        'tag' => NULL,
        'sha' => NULL,
      );
      $branch = $options['branch'];
      $tag = $options['tag'];
      $sha = $options['sha'];

      $posibilities = array_filter(array($branch, $tag, $sha));
      if (count($posibilities) > 1) {
        drush_log(dt('More than one of branch/tag/sha specified, using @selected.', array('@selected' => !empty($sha) ? 'sha' : 'tag')), 'warning');
      }
      elseif (count($posibilities) < 1) {
        return drush_set_error(dt('You must provide at least one of --branch, --tag or --sha.'));
      }

      // Even if we already have a SHA, we let it through rev-parse. Easy way to
      // check for existence and get it normalized to a full SHA.
      if ($this->shLocal('git rev-parse ' . end($posibilities))) {
        $sha = trim($this->shOutput());
      }
      else {
        if (!empty($sha)) {
          return drush_set_error(dt('Unknown SHA.'));
        }
        else {
          return drush_set_error(dt('Error finding SHA for ' . (!empty($branch) ? 'branch' : 'tag') . '.'));
        }
      }

      return $sha;
    }

    /**
     * Get the current git head revision.
     *
     * @param bool $no_cache
     *   Dont't use cache, but fetch anyway.
     */
    protected function getHead($no_cache = FALSE) {
      $name = $this->site['#name'];
      if (!isset(self::$headShas[$name]) || $no_cache) {
        self::$headShas[$name] = '';
        if ($this->sh('git rev-parse HEAD')) {
          self::$headShas[$name] = trim($this->shOutput());
        }
      }
      return self::$headShas[$name];
    }
  }
}

/**
 * Use a namespace to keep actions together.
 */
namespace Deployotron\Actions {
  use Deployotron\Action;

  /**
   * Sanity check.
   *
   * Check for locally modified files and do a dry-run checkout.
   */
  class SanityCheck extends Action {
    static protected $description = 'Check that the git repository is clean.';
    static protected $runMessage = 'Sanity checking';
    static protected $short = 'sanity check';

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      $fallback = FALSE;
      // http://stackoverflow.com/questions/3878624/how-do-i-programmatically-determine-if-there-are-uncommited-changes
      // claims this is the proper way. I'm inclined to agree.
      // However, these are newer additions to Git, so fallback to git status
      // if they fail.
      if (!$this->sh('git diff-files --quiet --ignore-submodules --')) {
        if ($this->shRc() != 129) {
          return drush_set_error(dt('Repository not clean.'));
        }
        $fallback = TRUE;
      }
      if (!$fallback) {
        if (!$this->sh('git diff-index --cached --quiet HEAD --ignore-submodules --')) {
          if ($this->shRc() != 129) {
            return drush_set_error(dt('Uncommitted changes in the index.'));
          }
          $fallback = TRUE;
        }
      }
      if ($fallback) {
        if (!$this->sh('git status -s')) {
          return drush_set_error(dt('Repository not clean.'));
        }
      }

      return TRUE;
    }
  }

  /**
   * Deploy code.
   */
  class DeployCode extends Action {
    static protected $runMessage = 'Deploying';
    static protected $killSwitch = 'no-deploy';
    static protected $short = 'deploy code';
    static protected $help = 'Checks out a specified branch/tag/sha on the site.';
    static protected $options = array(
      'branch' => 'Branch to check out.',
      'tag' => 'Tag to check out.',
      'sha' => 'SHA to check out.',
    );

    /**
     * {@inheritdoc}
     */
    public function getDescription() {
      $this->shLocal('git log --pretty="format:%an <%ae> @ %ci (%cr)%n%n%B" --color --no-walk ' . $this->sha);
      $commit = $this->shOutput();
      $this->shLocal('git diff --color --stat ' . $this->getHead() . ' ' . $this->sha);
      $stat = $this->shOutput();
      $stat = explode("\n", $stat);
      if (count($stat) > 20) {
        $stat = array_merge(array_slice($stat, 0, 8), array('', ' ...', ''), array_slice($stat, -8));
      }

      return dt("Run git fetch and check out @sha:\n!info", array('@sha' => $this->sha, '!info' => $commit . "\n\nAll changes:\n" . implode("\n", $stat)));
    }

    /**
     * {@inheritdoc}
     */
    public function validate() {
      $options = array(
        'branch' => drush_get_option('branch', NULL),
        'tag' => drush_get_option('tag', NULL),
        'sha' => drush_get_option('sha', NULL),
      );
      if ($this->sha = $this->pickSha($options)) {
        return TRUE;
      }
      return FALSE;
    }

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      // Some useful information for other actions.
      $state['requested_branch'] = drush_get_option('branch', NULL);
      $state['requested_tag'] = drush_get_option('tag', NULL);
      $state['requested_sha'] = drush_get_option('sha', NULL);

      if (!$this->sh('git fetch')) {
        $message = $this->shOutput();
        if (preg_match('/Permission denied \(publickey\)/', $message)) {
          drush_log(dt('Access denied to repository URL.'), 'error');
          drush_log(dt('Ensure that either that the user on the server has access to the repository, or use "ForwardAgent yes" in .ssh/config.'), 'error');
        }
        else {
          drush_print($message);
        }
        return drush_set_error(dt("Could not fetch from remote repository."));
      }

      if (!$this->sh('git checkout ' . $this->sha)) {
        $message = $this->shOutput();
        if (preg_match('/^fatal: reference is not a tree/', $message)) {
          drush_log(dt('Could not find SHA, did you forget to push?'), 'error');
        }
        else {
          drush_print($message);
        }
        return drush_set_error(dt('Could not checkout code.'));
      }

      // An extra safety check to make sure that things are as we think.
      $deployed_sha = $this->getHead(TRUE);
      if ($deployed_sha) {
        if ($deployed_sha != $this->sha) {
          return drush_set_error(dt('Code not properly deployed, head is at @sha now.', array('@sha' => $deployed_sha)));
        }
        else {
          drush_log(dt('HEAD now at @sha', array('@sha' => $deployed_sha)), 'status');
          $state['deployed_sha'] = $deployed_sha;
        }
      }
      else {
        drush_print($this->shOutput());
        return drush_set_error(dt('Error confirming that the code update succceded.'));
      }

      return TRUE;
    }
  }

  /**
   * Set site offline.
   */
  class SiteOffline extends Action {
    static protected $description = 'Set the site offline.';
    static protected $runMessage = 'Setting site offline';
    static protected $killSwitch = 'no-offline';
    static protected $short = 'set site offline';

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->drush('variable-set', array('maintainance_mode', 1))) {
        return drush_set_error(dt('Error setting site offline.'));
      }
      return TRUE;
    }

    /**
     * {@inheritdoc}
     */
    public function rollback() {
      // Use the online action as rollback.
      $online = new SiteOnline($this->site);
      $online->run(array());
    }
  }

  /**
   * Set site online.
   */
  class SiteOnline extends Action {
    static protected $description = 'Set the site online.';
    static protected $runMessage = 'Setting site online';
    static protected $killSwitch = 'no-offline';
    static protected $short = 'set site online';

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->drush('variable-set', array('maintainance_mode', 0))) {
        return drush_set_error(dt('Error setting site online.'));
      }
      return TRUE;
    }

    /**
     * {@inheritdoc}
     */
    public function rollback() {
      // Use the online action as rollback.
      $online = new SiteOffline($this->site);
      $online->run(array());
    }
  }

  /**
   * Backup database.
   */
  class BackupDatabase extends Action {
    static protected $runMessage = 'Dumping database';
    static protected $killSwitch = 'no-dump';
    static protected $short = 'dump database';
    static protected $help = 'Makes a SQL dump of the site database.';
    static protected $options = array(
      'dump-dir' => array(
        'description' => 'Directory for database dumps.',
        'example-value' => 'path',
      ),
    );

    /**
     * {@inheritdoc}
     */
    public function getDescription() {
      return dt("Dump database to @file.", array('@file' => $this->dumpFilename()));
    }

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->drush('sql-dump', array(), array('no-ordered-dump' => TRUE, 'result-file' => $this->dumpFilename()))) {
        return drush_set_error('Error dumping database.');
      }
      $state['database_dump'] = $this->dumpFilename();
      return TRUE;
    }

    /**
     * Figure out dump filename.
     */
    protected function dumpFilename() {
      // Because there can pass time between this is called first and last
      // (--confirm primarily).
      static $date;
      if (!$date) {
        $date = date('Y-m-d\TH:i:s');
      }

      return sprintf("%s/deploy.%s.%s.sql", drush_get_option('dump-dir', '/tmp'), $date, $this->getHead());
    }
  }

  /**
   * Restore database.
   */
  class RestoreDatabase extends Action {
    static protected $runMessage = 'Restoring database';
    static protected $short = 'restore database';
    static protected $options = array(
      'file' => array(
        'description' => 'Database dump file to restore.',
        'example-value' => 'filename',
      ),
    );

    /**
     * {@inheritdoc}
     */
    public function validate() {
      if (!drush_get_option('file', NULL)) {
        return drush_set_error(dt('Missing file.'));
      }

      return TRUE;
    }

    /**
     * {@inheritdoc}
     */
    public function getDescription() {
      return dt("Restore database from @file.", array('@file' => drush_get_option('file', NULL)));
    }

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->drush('sql-connect', array(), array('pipe' => TRUE))) {
        return drush_set_error('Error getting database connection setup.');
      }

      if (!is_string($this->drushResult['object'])) {
        return drush_set_error('Weird result from sql-connnect.');
      }

      $command = $this->drushResult['object'];

      if (!$this->sh($command . " < " . drush_get_option('file', NULL))) {
        drush_print($this->shOutput());
        return drush_set_error('Error restoring database.');
      }

      return TRUE;
    }
  }

  /**
   * Restart Apache2.
   */
  class RestartApache2 extends Action {
    static protected $description = 'Restart Apache2.';
    static protected $runMessage = 'Restarting Apache2';
    static protected $enableSwitch = 'restart-apache2';
    static protected $killSwitch = 'no-restart-apache2';
    static protected $short = 'restart apache2';
    static protected $options = array(
      'restart-apache2-command' => array(
        'description' => 'Override command used to restart Apache2.',
        'example-value' => 'command',
      ),
    );

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->sh(drush_get_option('restart-apache2-command', 'sudo service apache2 restart'))) {
        return drush_set_error(dt('Error restarting apache2.'));
      }
      return TRUE;
    }
  }

  /**
   * Restart Varnish.
   */
  class RestartVarnish extends Action {
    static protected $description = 'Restart Varnish.';
    static protected $runMessage = 'Restarting Varnish';
    static protected $enableSwitch = 'restart-varnish';
    static protected $killSwitch = 'no-restart-varnish';
    static protected $short = 'restart varnish';
    static protected $options = array(
      'restart-varnish-command' => array(
        'description' => 'Override command used to restart Varnish.',
        'example-value' => 'path',
      ),
    );

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->sh(drush_get_option('restart-varnish-command', 'sudo service varnish restart'))) {
        return drush_set_error(dt('Error restarting varnish.'));
      }
      return TRUE;
    }
  }

  /**
   * Update database.
   */
  class UpdateDatabase extends Action {
    static protected $description = 'Runs database updates (as with update.php).';
    static protected $runMessage = 'Running database updates';
    static protected $killSwitch = 'no-updb';
    static protected $short = 'update database schema';

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->drush('updb', array(), array('yes' => TRUE))) {
        return drush_set_error(dt('Error running database updates.'));
      }
      return TRUE;
    }
  }

  /**
   * Clear cache.
   */
  class ClearCache extends Action {
    static protected $description = 'Clear all Drupal caches.';
    static protected $runMessage = 'Clearing caches';
    static protected $short = 'cache clear';
    static protected $killSwitch = 'no-cc-all';

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->drush('cc', array('all'), array())) {
        return drush_set_error(dt('Error clearing cache.'));
      }
      return TRUE;
    }
  }

  /**
   * Prepare OMG.
   */
  class OMGPrepare extends Action {
    static protected $description = 'Prepare restore.';
    static protected $runMessage = 'Preparing';
    static protected $short = 'preparing';
    static protected $help = 'Prepares restoring by looking for dumps to import.';
    static protected $options = array(
      'dump-dir' => array(
        'description' => 'Directory for database dumps.',
        'example-value' => 'path',
      ),
    );

    /**
     * {@inheritdoc}
     */
    public function validate() {
      // Try to find some dumps and give them as options for restoring.
      $regex = '{^deploy\.(\d+-\d+-\d+T\d+:\d+:\d+)\.([0-9a-f]+)\.sql$}';
      $this->sh('ls ' . drush_get_option('dump-dir', '/tmp'));
      $listing = $this->shOutput();
      $dumps = array();
      foreach (array_reverse(explode("\n", $listing)) as $line) {
        if (preg_match($regex, $line, $matches)) {
          $dumps[$matches[1]] = $matches[2];
        }
      }

      if (!empty($dumps)) {
        $date = drush_choice($dumps, dt('Please select a dump.'), '!key (!value)');
        if ($date) {
          $sha = $dumps[$date];
          $file = 'deploy.' . $date . '.' . $sha . '.sql';
          // We simply set the options so the other actions will see them. The
          // DeployCode action will check that the SHA is available locally
          // before validating, so we'll avoid the worst if dumps get mixed up.
          drush_set_option('sha', $sha);
          drush_set_option('file', drush_get_option('dump-dir', '/tmp') . '/' . $file);
          return TRUE;
        }
        else {
          return drush_set_error(dt('Aborting.'));
        }
      }
      else {
        return drush_set_error(dt('No database dumps found.'));
      }
    }

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      // Doing nothing.
      return TRUE;
    }
  }

  /**
   * Run Drake.
   */
  class RunDrakePostUpdate extends Action {
    static protected $description = 'Run drake post-update.';
    static protected $runMessage = 'Running drake post-update';
    static protected $short = 'run drake post-update';
    static protected $enableSwitch = 'drake-post-update';

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!$this->drush('drake', array('post-update'), array('no-task-ok' => TRUE))) {
        return drush_set_error(dt('Error runninng drake.'));
      }
      return TRUE;
    }
  }

  /**
   * Create a VERSION.txt file.
   */
  class CreateVersionTxt extends Action {
    static protected $description = 'Create VERSION.txt.';
    static protected $runMessage = 'Creating VERSION.txt';
    static protected $short = 'create VERSION.txt';
    static protected $killSwitch = 'no-create-version-txt';
    static protected $versionTxt = NULL;

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!empty($state['deployed_sha'])) {
        // Ask git which tags points to this commit.
        $this->shLocal('git tag --points-at ' . $state['deployed_sha']);
        $tags = implode(', ', $this->shOutputArray());

        $version_txt = array();
        $version_txt[] = 'Deployment info';
        $version_txt[] = '---------------';
        if (!empty($state['requested_branch'])) {
          $version_txt[] = 'Branch: ' . $state['requested_branch'];
        }
        if (!empty($tags)) {
          $version_txt[] = 'Tags: ' . $tags;
        }
        $version_txt[] = 'SHA: ' . $state['deployed_sha'];
        $version_txt[] = 'Time of deployment: ' . date('r');
        $version_txt[] = 'Deployer: ' . $_SERVER['USER'] . '@' . php_uname('n');

        // Delete any pre-existing VERSION.txt file and create a new one.
        $this->sh('rm VERSION.txt');
        // You'd think that echo would do the job, but it's not consistent
        // across shells. Bash and most /bin/echo requires the -n option to
        // expand \n to a newline, while /bin/sh built-in echo doesn't and
        // prints the -n as part of the output. But printf works the same and is
        // POSIX 7, which should cover our bases.
        $this->sh('printf ' . escapeshellarg(implode("\\n", $version_txt) . "\\n") . ' > VERSION.txt');
      }
      else {
        drush_log(dt('No version deployed, not creating/updating VERSION.txt.'), 'warning');
      }
      return TRUE;
    }
  }

  /**
   * Send a notification to Flowdock.
   */
  class FlowdockNotificaton extends Action {
    static protected $description = 'Send Flowdock notification.';
    static protected $runMessage = 'Sending Flowdock notification.';
    static protected $short = 'send Flowdock notification';
    static protected $enableSwitch = 'flowdock-token';
    static protected $options = array(
      'flowdock-token' => array(
        'description' => 'Flowdock token.',
        'example-value' => 'token',
      ),
    );

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!empty($state['deployed_sha'])) {
        $this->shLocal('git tag --points-at ' . $state['deployed_sha']);
        $tags = implode(', ', $this->shOutputArray());

        $subject = 'Deployment to ' . $this->site['remote-user'] . '@' .
          $this->site['remote-host'] . ':' . $this->site['root'];

        $body = 'SHA: ' . $state['deployed_sha'] .
          (!empty($state['requested_branch']) ? '<br />Branch: ' . $state['requested_branch'] : '') .
          (!empty($tags) ? '<br />Tags: ' . $tags : '');

        $data = array(
          'source' => 'Deployotron',
          'from_address' => 'deployotron@reload.dk',
          'subject' => $subject,
          'content' => $body,
          'tags' => array('deployotron', $this->site['#name']),
        );
        $data = json_encode($data);

        $service_url = 'https://api.flowdock.com/v1/messages/team_inbox/' . drush_get_option('flowdock-token', '');

        $curl = curl_init($service_url);
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($curl, CURLOPT_POST, TRUE);
        curl_setopt($curl, CURLOPT_POSTFIELDS, $data);
        curl_setopt($curl, CURLOPT_HTTPHEADER, array(
            'Content-Type: application/json',
            'Content-Length: ' . strlen($data),
          )
        );

        $curl_response = curl_exec($curl);
        if ($curl_response != '{}') {
          drush_log(dt('Unexpected response from Flowdock: !response', array('!response' => $curl_response)), 'warning');
        }
        curl_close($curl);
      }
      else {
        drush_log(dt('No version deployed, not sending Flowdock notification.'), 'warning');
      }
      return TRUE;
    }
  }

  /**
   * Send a notification to New Relic.
   */
  class NewRelicNotificaton extends Action {
    static protected $description = 'Send New Relic notification.';
    static protected $runMessage = 'Sending New Relic notification.';
    static protected $short = 'send new relic notification';
    static protected $enableSwitch = 'newrelic-api-key';
    static protected $options = array(
      'newrelic-app-name' => array(
        'description' => 'New Relic application name.',
        'example-value' => 'name',
      ),
      'newrelic-app-id' => array(
        'description' => 'New Relic application id.',
        'example-value' => 'id',
      ),
      'newrelic-api-key' => array(
        'description' => 'New Relic API key.',
        'example-value' => 'key',
      ),
    );

    /**
     * {@inheritdoc}
     */
    public function validate() {
      if (!drush_get_option('newrelic-app-name', NULL) && !drush_get_option('newrelic-app-id', NULL)) {
        return drush_set_error('Need at least one of --newrelic-app-name or --newrelic-app-id');
      }
      // The api-key must have been specified as its the enable switch.
      return TRUE;
    }

    /**
     * {@inheritdoc}
     */
    public function run($state) {
      if (!empty($state['deployed_sha'])) {
        $this->shLocal('git tag --points-at ' . $state['deployed_sha']);
        $tags = implode(', ', $this->shOutputArray());

        $body = 'SHA: ' . $state['deployed_sha'] .
          (!empty($state['requested_branch']) ? "\nBranch: " . $state['requested_branch'] : '') .
          (!empty($tags) ? "\nTags: " . $tags : '');

        $deployment = array(
          'description' => $body,
          'revision' => $state['deployed_sha'],
          // @todo 'changelog' => '',
          'user' => $_SERVER['USER'] . '@' . php_uname('n'),
        );

        $app_name = drush_get_option('newrelic-app-name', NULL);
        if ($app_name) {
          $deployment['app_name'] = $app_name;
        }

        $app_id = drush_get_option('newrelic-app-id', NULL);
        if ($app_id) {
          $deployment['application_id'] = $app_id;
        }

        $data = http_build_query(array('deployment' => $deployment));
        $service_url = 'https://api.newrelic.com/deployments.xml';

        $curl = curl_init($service_url);
        curl_setopt($curl, CURLOPT_RETURNTRANSFER, TRUE);
        curl_setopt($curl, CURLOPT_POST, TRUE);
        curl_setopt($curl, CURLOPT_POSTFIELDS, $data);
        curl_setopt($curl, CURLOPT_HTTPHEADER, array(
            'Content-Type: application/x-www-form-urlencoded',
            'Content-Length: ' . strlen($data),
            'x-api-key: ' . drush_get_option('newrelic-api-key'),
          )
        );

        $curl_response = curl_exec($curl);
        if ($curl_response === FALSE) {
          drush_log(dt('Curl failed: !response', array('!response' => curl_error($curl))), 'error');
        }
        elseif (!$curl_response) {
          drush_log(dt('New Relic notification failed for some reason.'), 'error');
        }
        curl_close($curl);
      }
      else {
        drush_log(dt('No version deployed, not sending New Relic notification.'), 'warning');
      }
      return TRUE;
    }
  }
}
