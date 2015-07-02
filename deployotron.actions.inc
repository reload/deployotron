<?php
/**
 * @file
 * Actions for Deployotron.
 */

namespace Deployotron {

  define('COLOR_RED', "\033[1;31;40m\033[1m");
  define('COLOR_YELLOW', "\033[1;33;40m\033[1m");
  define('COLOR_GREEN', "\033[1;32;40m\033[1m");
  define('COLOR_RESET', "\033[0m");

  /**
   * Creates actions to run.
   */
  class ActionFactory {
    static protected $actionMapping = array(
      'deploy' => array(
        'SanityCheck',
        'SiteOffline',
        'BackupDatabase',
        'DeployCode',
        'CreateVersionTxt',
        'UpdateDatabase',
        'ClearCache',
        'SiteOnline',
        'PurgeDatabaseBackups',
        'FlowdockNotificaton',
        'NewRelicNotificaton',
      ),
      'omg' => array(
        'SiteOffline',
        'OMGPrepare',
        'DeployCode',
        'CreateVersionTxt',
        'RestoreDatabase',
        'SiteOnline',
        'ClearCache',
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
          $actions = array_merge($actions,
                                 static::getActionCommands('pre', $action, $site),
                                 array($action),
                                 static::getActionCommands('post', $action, $site));
        }
        return $actions;
      }
    }

    /**
     * Gets pre/post commands for an action.
     */
    static protected function getActionCommands($type, $action, $site) {
      $actions = array();
      $switch = $type . '-' . $action->getSwitchSuffix();
      if ($action->enabled() && !drush_get_option('no-' . $switch, FALSE)) {
        // @todo drush_get_option_list() doesn't play well with commas.
        if ($commands = drush_get_option_list($switch, FALSE)) {
          foreach ($commands as $command) {
            if (!empty($command)) {
              $actions[] = new Actions\CommandAction($site, $command);
            }
          }
        }
      }

      return $actions;
    }

    /**
     * Output help on actions and options.
     */
    static public function getHelp() {
      $all_actions = array();
      drush_print('Commands');
      drush_print('--------');
      foreach (static::$actionMapping as $name => $actions) {
        drush_print(wordwrap(dt('@name runs the actions: @actions', array(
                '@name' => $name,
                '@actions' => implode(', ', $actions),
              ))));
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
        $action = new $class_name('@self');
        drush_print($action->getHelp());
        drush_print();

        // Print options.
        $options = $action->getOptions();
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
    protected $short = "incompletely implemented";

    /**
     * Default run message.
     */
    protected $runMessage = "Running action.";

    /**
     * Options for this action.
     */
    protected $options = array();

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

    /**
     * Construct a new Action object.
     */
    public function __construct($site) {
      $this->site = $site;

      if ($this->short == "incompletely implemented") {
        drush_log(dt('Incomplete action, missing short description: @action', array('@action' => get_class($this))), 'warning');
      }

      if (!isset($this->runMessage)) {
        $this->runMessage = 'Running ' . $this->short;
      }
    }

    /**
     * Get help description.
     */
    public function getHelp() {
      if (isset($this->help)) {
        return $this->help;
      }
      if (isset($this->description)) {
        return $this->description;
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
    public function getOptions() {
      $options = array();
      if (isset($this->killSwitch)) {
        $options += array(
          $this->killSwitch => "Don't " . $this->short . (isset($this->enableSwitch) ? ' (overrides --' . $this->enableSwitch . ')' : '') . '.',
        );
      }

      $options += $this->options;

      $prest_options = array(
        'pre/post-*' => array(
          'pre-' . $this->getSwitchSuffix() => "Before " . $this->short . ".",
          'post-' . $this->getSwitchSuffix() => "After " . $this->short . ".",
        ),
      );

      if (isset($this->enableSwitch)) {
        $sub_options = $options;
        $options = array();
        // If the action options defines the enable switch, use it so
        // description and possible example-values propergate.
        if (isset($sub_options[$this->enableSwitch])) {
          $options[$this->enableSwitch] = $sub_options[$this->enableSwitch];
          unset($sub_options[$this->enableSwitch]);
        }
        else {
          $options[$this->enableSwitch] = ucfirst($this->short) . '.';
        }
        $options['pre/post-*'] = "Commands to run before/after action.";
        return array(
          'options' => $options,
          'sub-options' => (!empty($sub_options) ? array($this->enableSwitch => $sub_options) : array()) + $prest_options,
        );
      }

      $options['pre/post-*'] = "Commands to run before/after action.";
      return array('options' => $options, 'sub-options' => $prest_options);
    }

    /**
     * Get the switch suffix for --pre-/--post- switches.
     */
    public function getSwitchSuffix() {
      if (isset($this->switchSuffix)) {
        return $this->switchSuffix;
      }
      elseif (isset($this->enableSwitch)) {
        return $this->enableSwitch;
      }
      elseif (isset($this->killSwitch)) {
        return preg_replace('/^no-/', '', $this->killSwitch);
      }
      else {
        return preg_replace(array('/[^a-z0-9 ]/', '/ /'), array('', '-'), strtolower($this->short));
      }
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
      return $this->short;
    }

    /**
     * Return the message to print when running this action.
     *
     * @return string
     *   The Message (by Grandmaster Flash).
     */
    public function getRunMessage() {
      return $this->runMessage;
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
      if (isset($this->description)) {
        return $this->description;
      }
      return dt("Run the @short action.", array('@short' => $this->short));
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
    public function run(array $state) {
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
      if ($this->short == "incompletely implemented") {
        return FALSE;
      }
      // Disable if there is a killswitch and it is set.
      if (isset($this->killSwitch) && drush_get_option($this->killSwitch, FALSE)) {
        return FALSE;
      }
      // If there is an enable switch, let that decide.
      if (isset($this->enableSwitch)) {
        return drush_get_option($this->enableSwitch, FALSE);
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
     * @return string
     *   The SHA to deploy.
     */
    protected function pickSha($options = array()) {
      $options = array(
        'branch' => drush_get_option('branch', NULL),
        'tag' => drush_get_option('tag', NULL),
        'sha' => drush_get_option('sha', NULL),
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

      // Use rev-list to ensure we get a full SHA rather than branch/tag/partial
      // SHA. Ensures the existence of the branch/tag/SHA, and transforms
      // annotated tag SHAs to the commit they're based on (so the repo will
      // actually have the SHA we asked for checked out).
      if ($this->shLocal('git rev-list -1 ' . end($posibilities) . ' --')) {
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
          $sha = trim($this->shOutput());
          // Confirm that we can see the HEAD locally.
          if ($this->shLocal('git show ' . $sha)) {
            self::$headShas[$name] = $sha;
          }
          else {
            drush_log(dt("Could not find the deployed HEAD (@sha) locally, you pulled recently?", array('@sha' => $sha)), 'warning');
          }
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
   * Generic command action.
   *
   * Automatically instantiated from pre/post-* options.
   */
  class CommandAction extends Action {
    /**
     * Create action.
     */
    public function __construct($site, $command) {
      $this->description = 'Run command: ' . $command;
      $this->runMessage = 'Running command: ' . $command;
      $this->short = 'run ' . $command;

      $this->command = $command;
      parent::__construct($site);
    }

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
      $success = $this->sh($this->command);
      drush_print($this->shOutput());
      if (!$success) {
        return drush_set_error(dt('Error running command "@command"', array('@command' => $this->command)));
      }
      return TRUE;
    }
  }

  /**
   * Sanity check.
   *
   * Check for locally modified files and do a dry-run checkout.
   */
  class SanityCheck extends Action {
    protected $runMessage = 'Fetching and sanity checking';
    protected $short = 'sanity check';
    protected $help = 'Fetches the new code and checks that the repository is clean.';
    protected $options = array(
      'insanity' => "Don't check that the server repository is clean. Might cause data loss.",
    );

    /**
     * {@inheritdoc}
     */
    public function getDescription() {
      if (drush_get_option('insanity', FALSE)) {
        return COLOR_YELLOW . dt('Skipping sanity check of deployed repo. Might cause data loss or Git failures.') . COLOR_RESET;
      }

      return dt('Run git fetch and check that the deployed git repository is clean.');
    }

    /**
     * {@inheritdoc}
     */
    public function validate() {
      if ($this->sha = $this->pickSha()) {
        return TRUE;
      }
      return FALSE;
    }

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
      // Fetch the latest changes from upstream.
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

      // Check that we can see the sha we want to deploy.
      if (!$this->sh('git log -1 ' . $this->sha . ' --')) {
        $message = $this->shOutput();
        if (preg_match('/^fatal: bad object/', $message)) {
          return drush_set_error(dt('Could not find SHA, did you forget to push?'));
        }
        drush_print($message);
        return drush_set_error(dt('Could not find SHA remotely.'));
      }

      if (drush_get_option('insanity', FALSE)) {
        drush_log(dt('Skipping sanity check of deployed repo.'), 'warning');
        return TRUE;
      }
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
        if (!$this->sh('git status -s --untracked-files=no')) {
          return drush_set_error(dt('Error running git status -s --untracked-files=no.'));
        }
        $output = trim($this->shOutput());
        if (!empty($output)) {
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
    protected $runMessage = 'Deploying';
    protected $killSwitch = 'no-deploy';
    protected $short = 'deploy code';
    protected $help = 'Checks out a specified branch/tag/sha on the site.';
    protected $options = array(
      'branch' => 'Branch to check out.',
      'tag' => 'Tag to check out.',
      'sha' => 'SHA to check out.',
    );

    /**
     * {@inheritdoc}
     */
    public function getDescription() {
      $head = $this->getHead();
      if (!$head) {
        return COLOR_YELLOW . dt("Cannot find the deployed HEAD.") . COLOR_RESET;
      }

      // Collect info and log entry for the commit we're deploying.
      $this->shLocal('git log --pretty="format:%an <%ae> @ %ci (%cr)%n%n%B" --color --no-walk ' . $this->sha);
      $commit_info = $this->shOutput();

      // Collect info and log entry for the deployed head.
      $this->shLocal('git log --pretty="format:%an <%ae> @ %ci (%cr)%n%n%B" --color --no-walk ' . $head);
      $head_info = $this->shOutput();

      // Create diffstat between the deployed commit and the one we're
      // deploying.
      $this->shLocal('git diff --color --stat ' . $head . ' ' . $this->sha);
      $stat = $this->shOutput();
      $stat = explode("\n", $stat);
      if (count($stat) > 20) {
        $stat = array_merge(array_slice($stat, 0, 8), array('', ' ...', ''), array_slice($stat, -8));
      }

      $desc = dt("Check out @sha:\n", array('@sha' => $this->sha));
      $desc .= $commit_info . "\n\n";

      $desc .= dt("Currently deployed: @sha\n", array('@sha' => $head));
      $desc .= $head_info . "\n\n";

      $desc .= dt("All changes:\n!changes", array('!changes' => implode("\n", $stat)));

      return $desc;
    }

    /**
     * {@inheritdoc}
     */
    public function validate() {
      if ($this->sha = $this->pickSha()) {
        return TRUE;
      }
      return FALSE;
    }

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
      // Some useful information for other actions.
      $state['requested_branch'] = drush_get_option('branch', NULL);
      $state['requested_tag'] = drush_get_option('tag', NULL);
      $state['requested_sha'] = drush_get_option('sha', NULL);

      if (!$this->sh('git checkout ' . $this->sha)) {
        drush_print($this->shOutput());
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
    protected $description = 'Set the site offline.';
    protected $runMessage = 'Setting site offline';
    protected $killSwitch = 'no-offline';
    protected $short = 'set site offline';

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
      if (!$this->drush('variable-set', array('maintenance_mode', 1))) {
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
    protected $description = 'Set the site online.';
    protected $runMessage = 'Setting site online';
    protected $killSwitch = 'no-offline';
    protected $short = 'set site online';
    protected $switchSuffix = 'online';

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
      if (!$this->drush('variable-set', array('maintenance_mode', 0))) {
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
    protected $runMessage = 'Dumping database';
    protected $killSwitch = 'no-dump';
    protected $short = 'dump database';
    protected $help = 'Makes a SQL dump of the site database.';
    protected $options = array(
      'dump-dir' => array(
        'description' => 'Directory for database dumps, defaults to /tmp.',
        'example-value' => 'path',
      ),
    );

    /**
     * Get the name of a dump.
     */
    public static function filename($alias, $date_str, $sha) {
      return sprintf("%s/deploy.%s.%s.%s.sql", drush_get_option('dump-dir', '/tmp'), $alias, $date_str, $sha);
    }

    /**
     * {@inheritdoc}
     */
    public function getDescription() {
      return dt("Dump database to @file.", array('@file' => $this->dumpFilename()));
    }

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
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

      return static::filename($this->site['#name'], $date, $this->getHead());
    }
  }

  /**
   * Restore database.
   */
  class RestoreDatabase extends Action {
    protected $runMessage = 'Restoring database';
    protected $short = 'restore database';
    protected $options = array(
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
    public function run(array $state) {
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
   * Purge old backups.
   */
  class PurgeDatabaseBackups extends Action {
    protected $runMessage = 'Purging old database dumps.';
    protected $short = 'purge old dumps';
    protected $options = array(
      'num-dumps' => array(
        'description' => 'Number of database dumps to keep. 0 for unlimited.',
        'example-value' => '5',
      ),
    );
    protected $switchSuffix = 'purge';

    /**
     * Dumps to delete.
     */
    protected $deleteDumps = array();

    /**
     * {@inheritdoc}
     */
    public function validate() {
      $dumping = TRUE;
      if (drush_get_option('no-dump', FALSE)) {
        $dumping = FALSE;
      }
      $max_dumps = drush_get_option('num-dumps', 5);

      if ($max_dumps > 0) {
        // If we're dumping a new dump, we need to keep one less than the max to
        // make room for the new one.
        $keep = $max_dumps - ($dumping ? 1 : 0);
        $this->sh('ls ' . BackupDatabase::filename($this->site['#name'], '*', '*'));
        $dumps = $this->shOutputArray();
        if (count($dumps) > $keep) {
          // Reverse sort to get the newest first.
          rsort($dumps);
          $this->deleteDumps = array_slice($dumps, $keep);
        }
      }

      return TRUE;
    }

    /**
     * {@inheritdoc}
     */
    public function getDescription() {
      if (count($this->deleteDumps)) {
        return dt("Purge the following dump files:\n@files.", array('@files' => implode("\n", $this->deleteDumps)));

      }
      return dt("Not purging any dumps.");
    }

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
      if (count($this->deleteDumps)) {
        $this->sh('rm ' . implode(" ", array_map('drush_escapeshellarg', $this->deleteDumps)));
      }

      // We don't consider failure to delete dumps serious enough to fail the
      // deployment.
      return TRUE;
    }
  }

  /**
   * Update database.
   */
  class UpdateDatabase extends Action {
    protected $description = 'Runs database updates (as with update.php).';
    protected $runMessage = 'Running database updates';
    protected $killSwitch = 'no-updb';
    protected $short = 'update database schema';

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
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
    protected $description = 'Clear all Drupal caches.';
    protected $runMessage = 'Clearing caches';
    protected $short = 'cache clear';
    protected $killSwitch = 'no-cc-all';

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
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
    protected $description = 'Prepare restore.';
    protected $runMessage = 'Preparing';
    protected $short = 'preparing';
    protected $help = 'Prepares restoring by looking for dumps to import.';
    protected $options = array(
      'dump-dir' => array(
        'description' => 'Directory for database dumps.',
        'example-value' => 'path',
      ),
    );
    protected $switchSuffix = 'prepare';

    /**
     * {@inheritdoc}
     */
    public function validate() {
      // Try to find some dumps and give them as options for restoring.
      $regex = '{^deploy\.' . preg_quote($this->site['#name']) . '\.(\d+-\d+-\d+T\d+:\d+:\d+)\.([0-9a-f]+)\.sql$}';
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
          $file = 'deploy.' . $this->site['#name'] . '.' . $date . '.' . $sha . '.sql';
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
    public function run(array $state) {
      // Doing nothing.
      return TRUE;
    }
  }

  /**
   * Create a VERSION.txt file.
   */
  class CreateVersionTxt extends Action {
    protected $description = 'Create VERSION.txt.';
    protected $runMessage = 'Creating VERSION.txt';
    protected $short = 'create VERSION.txt';
    protected $killSwitch = 'no-create-version-txt';
    protected $switchSuffix = 'version-txt';

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
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
    protected $description = 'Send Flowdock notification.';
    protected $runMessage = 'Sending Flowdock notification.';
    protected $short = 'send Flowdock notification';
    protected $enableSwitch = 'flowdock-token';
    protected $options = array(
      'flowdock-token' => array(
        'description' => 'Flowdock token.',
        'example-value' => 'token',
      ),
    );
    protected $switchSuffix = 'flowdock';

    /**
     * {@inheritdoc}
     */
    public function run(array $state) {
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
          'tags' => array('deployotron', $this->getFlowdockTag()),
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

    /**
     * Get a Flowdock tag for the site.
     */
    private function getFlowdockTag() {
      return preg_replace('/[^[:alnum:]_\-]/', '_', $this->site['#name']);
    }
  }

  /**
   * Send a notification to New Relic.
   */
  class NewRelicNotificaton extends Action {
    protected $description = 'Send New Relic notification.';
    protected $runMessage = 'Sending New Relic notification.';
    protected $short = 'send new relic notification';
    protected $enableSwitch = 'newrelic-api-key';
    protected $options = array(
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
    protected $switchSuffix = 'newrelic';

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
    public function run(array $state) {
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
