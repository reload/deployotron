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
        'RestartApache2',
        'UpdateDatabase',
        'RunDrakePostUpdate',
        'ClearCache',
        'SiteOnline',
        'RestartVarnish',
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
  }

  /**
   * Base class for actions.
   */
  abstract class Action {
    /**
     * Set a default short description. Also @see isKillSwitched().
     */
    static protected $short = "Incompletly implemented action.";

    static protected $runMessage = "Running action.";

    /**
     * The site this action works on.
     */
    protected $site;

    /**
     * Result object of last drush command.
     */
    protected $drushResult;

    /**
     * Shas of checkouts of aliases.
     */
    static protected $headShas = array();

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
     * Actions are encuraged to override this to provide a more specific
     * description of the effect of the action. Used when the user provides the
     * --confirm switch.
     *
     * @return string
     *   Description of the action.
     */
    public function describe() {
      return dt("Run the @short action.", array('@short' => static::$short));
    }

    /**
     * Get the command line options for this action.
     *
     * @return array
     *   Options as defined on commands in hook_drush_command().
     */
    public function getOptions() {
      $options = array();
      if (isset(static::$killSwitch)) {
        $options += array(
          static::$killSwitch => "Don't " . static::$short . (isset(static::$enableSwitch) ? ' (overrides --' . static::$enableSwitch . ')' : '') . '.',
        );
      }
      if (isset(static::$enableSwitch)) {
        $options += array(
          static::$enableSwitch => ucfirst(static::$short) . '.',
        );
      }
      return $options;
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
     * @param array $site
     *   Site alias to run action on.
     *
     * @return bool
     *   Success or failure.
     */
    public function run($site) {
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
      $exec = drush_shell_proc_build($this->site, $command, TRUE);
      return $this->shLocal($exec);
    }

    /**
     * Get the output of the most recent command.
     *
     * @return string
     *   The output.
     */
    protected function shOutput() {
      return implode("\n", drush_shell_exec_output());
    }

    /**
     * Execute a sh locally.
     *
     * @param string $command
     *   Command to run.
     *
     * @return bool
     *   Whether the command succeeded.
     */
    protected function shLocal($command) {
      if (!drush_shell_exec($command)) {
        return FALSE;
      }
      return TRUE;
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
     * Get the current git head revision.
     */
    protected function getHead() {
      $name = $this->site['#name'];
      if (!isset(self::$headShas[$name])) {
        self::$headShas[$name] = '';
        $exec = drush_shell_proc_build($this->site, "git rev-parse HEAD", TRUE);
        // Return code 0 = success.
        if (drush_shell_exec($exec)) {
          self::$headShas[$name] = trim(implode("\n", drush_shell_exec_output()));
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
    static protected $runMessage = 'Sanity checking';
    static protected $short = 'sanity check';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return dt("Check that the git repository is clean.");
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
      if (!$this->sh('git status -s')) {
        return drush_set_error(dt('Repository not clean.'));
      }

      return TRUE;
    }
  }

  /**
   * Deploy code.
   */
  class DeployCode extends Action {
    static protected $runMessage = 'Deploying';
    static protected $short = 'deploy code';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      $this->shLocal('git show --pretty="format:%an <%ae> @ %ci (%cr)%n%n%B" --color --stat ' . $this->sha);
      return dt("Run git fetch and check out @sha:\n!info", array('@sha' => $this->sha, '!info' => $this->shOutput()));
    }

    /**
     * {@inheritdoc}
     */
    public function getOptions() {
      return parent::getOptions() + array(
        'branch' => 'Branch to check out.',
        'tag' => 'Tag to check out.',
        'sha' => 'SHA to check out.',
      );
    }

    /**
     * {@inheritdoc}
     */
    public function validate() {
      $branch = drush_get_option('branch', NULL);
      $tag = drush_get_option('tag', NULL);
      $sha = drush_get_option('sha', NULL);

      $posibilities = array_filter(array($branch, $tag, $sha));
      if (count($posibilities) > 1) {
        return drush_set_error(dt('--branch, --tag and --sha are mutually exclusive.'));
      }
      elseif (count($posibilities) < 1) {
        return drush_set_error(dt('You must provide at least one of --branch, --tag or --sha.'));
      }

      // Even if we already have a SHA, we let it through rev-parse. Easy way to
      // check for existence and get it normalized to a full SHA.
      if ($this->shLocal('git rev-parse ' . reset($posibilities))) {
        $this->sha = trim($this->shOutput());
      }
      else {
        if (!empty($sha)) {
          return drush_set_error(dt('Unknown SHA.'));
        }
        else {
          return drush_set_error(dt('Error finding SHA for ' . (!empty($branch) ? 'branch' : 'tag') . '.'));
        }
      }

      return TRUE;
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
      if (!$this->sh('git fetch')) {
        drush_print($this->shOutput());
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
      if ($this->shLocal('git rev-parse HEAD')) {
        $deployed_sha = trim($this->shOutput());
        if ($deployed_sha != $this->sha) {
          return drush_set_error(dt('Code not properly deployed Head is at @sha now.', array('@sha' => $deployed_sha)));
        }
        else {
          drush_log(dt('HEAD now at @sha', array('@sha' => $deployed_sha)), 'status');
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
    static protected $runMessage = 'Setting site offline';
    static protected $short = 'set site offline';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return dt("Set the site offline.");
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
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
      $online->run();
    }
  }

  /**
   * Set site online.
   */
  class SiteOnline extends Action {
    static protected $runMessage = 'Setting site online';
    static protected $short = 'set site online';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return dt("Set the site online.");
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
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
      $online->run();
    }
  }

  /**
   * Backup database.
   */
  class BackupDatabase extends Action {
    static protected $runMessage = 'Dumping database';
    static protected $killSwitch = 'no-dump';
    static protected $short = 'dump database';

    /**
     * {@inheritdoc}
     */
    public function getOptions() {
      return parent::getOptions() + array(
        'dump-dir' => 'Directory for database dumps.',
      );
    }

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return dt("Dump database to @file.", array('@file' => $this->dumpFilename()));
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
      if (!$this->drush('sql-dump', array(), array('no-ordered-dump' => TRUE, 'result-file' => $this->dumpFilename()))) {
        return drush_set_error('Error dumping database.');
      }
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

    /**
     * {@inheritdoc}
     */
    public function getOptions() {
      return parent::getOptions() + array(
        'file' => 'Database dump file to restore.',
      );
    }
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
    public function describe() {
      return dt("Restore database from @file.", array('@file' => drush_get_option('file', NULL)));
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
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
   * Restart Apache2.
   */
  class RestartApache2 extends Action {
    static protected $runMessage = 'Restarting Apache2';
    static protected $enableSwitch = 'restart-apache2';
    static protected $killSwitch = 'no-restart-apache2';
    static protected $short = 'restart apache2';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return dt("Restart Apache2.");
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
      if (!$this->sh('sudo service apache2 restart')) {
        return drush_set_error(dt('Error restarting apache2.'), 'error');
      }
      return TRUE;
    }
  }

  /**
   * Restart Varnish.
   */
  class RestartVarnish extends Action {
    static protected $runMessage = 'Restarting Varnish';
    static protected $enableSwitch = 'restart-varnish';
    static protected $killSwitch = 'no-restart-varnish';
    static protected $short = 'restart varnish';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return dt("Restart Varnish");
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
      if (!$this->sh('sudo service varnish restart')) {
        return drush_set_error(dt('Error restarting varnish.'), 'error');
      }
      return TRUE;
    }
  }

  /**
   * Update database.
   */
  function deployotron_deploy_action_update_database($site, $settings) {
  }
  class UpdateDatabase extends Action {
    static protected $runMessage = 'Running database updates';
    static protected $killSwitch = 'no-updb';
    static protected $short = 'update database schema';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return dt("Update database schema.");
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
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
    static protected $runMessage = 'Clearing caches';
    static protected $short = 'cache clear';
    static protected $killSwitch = 'no-cc-all';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return "Clear all Drupal caches.";
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
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
    static protected $runMessage = 'Preparing';
    static protected $short = 'preparing';

    /**
     * {@inheritdoc}
     */
    public function getOptions() {
      return parent::getOptions() + array(
        'dump-dir' => 'Directory for database dumps.',
      );
    }

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
    public function describe() {
      return "Preparing restore.";
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
      // Doing nothing.
      return TRUE;
    }
  }

  /**
   * Run Drake.
   */
  class RunDrakePostUpdate extends Action {
    static protected $runMessage = 'Running drake post-update';
    static protected $short = 'run drake post-update';
    static protected $enableSwitch = 'drake-post-update';

    /**
     * {@inheritdoc}
     */
    public function describe() {
      return "Run drake post-update.";
    }

    /**
     * {@inheritdoc}
     */
    public function run() {
      if (!$this->drush('drake', array('post-update'), array('no-task-ok' => TRUE))) {
        return drush_set_error(dt('Error runninng drake.'));
      }
      return TRUE;
    }
  }
}
