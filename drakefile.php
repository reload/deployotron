<?php

/**
 * @file
 * Semi generic deployment script.
 *
 * Use like this:
 * $ drush drake deploy @prod
 *
 * A specific tag to deploy can be added to the command line.
 *
 * Add no-dump=1 to skip dumping of database.
 *
 * This should go in tandem with an alias file which define the relevant servers
 * and, ideally, default branches to deploy.
 *
 * In order to be able to restart apache2/varnish, use the following command to
 * edit a sudoers.d config file:
 *
 * sudo visudo -f /etc/sudoers.d/deployotron
 *
 * And add the following to the file:
 *
 * deploy_user          ALL=(root) NOPASSWD: /usr/sbin/service apache2 restart,/usr/sbin/service varnish restart
 *
 */

$api = 1;

$tasks['deploy'] = array(
  'action' => 'run-deploy',
  'alias' => drake_argument(1, 'Alias to deploy to.'),
  'tag' => drake_argument_optional(2, 'Tag or branch to build (prefix branches with "origin/").'),
  'dump-dir' => context_optional('dump-dir', NULL, 'Directory for DB dump.'),
  'no-dump' => context_optional('no-dump', NULL, 'Skip DB dump.'),
  'no-updb' => context_optional('no-updb', NULL, 'Skip database update.'),
  'no-cc-all' => context_optional('no-cc-all', NULL, 'Skip cache clear.'),
);

$tasks['deployotron-install'] = array(
  'action' => 'drush',
  'commands' => array(
    array(
      'command' => 'dl',
      'args' => array(
        0 => 'gittyup-6.x-1.x',
        'destination' => 'sites/all/drush',
      ),
    ),
    array(
      'command' => 'dl',
      'args' => array(
        0 => 'drush_drake-7.x-1.7',
        'destination' => 'sites/all/drush',
      ),
    ),
  ),
);

$actions['run-deploy'] = array(
  'default_message' => 'Deploying...',
  'callback' => 'run_deploy',
  'parameters' => array(
    'alias' => 'Alias to deploy to.',
    'tag' => array(
      'description' => 'Tag or branch to build (prefix branches with "origin/").',
      'default' => NULL,
    ),
    'dump-dir' => array(
      'description' => 'Directory to store DB dumps.',
      'default' => NULL,
    ),
    'no-dump' => array(
      'description' => 'Skip DB dump.',
      'default' => FALSE,
    ),
    'no-updb' => array(
      'description' => 'Skip database update.',
      'default' => FALSE,
    ),
    'no-cc-all' => array(
      'description' => 'Skip cache clear.',
      'default' => FALSE,
    ),
  ),
);

/**
 * Action callback.
 *
 * Runs the deployment.
 */
function run_deploy($context) {
  $site = drush_sitealias_get_record($context['alias']);
  if (empty($site)) {
    return drake_action_error(dt('Invalid alias.'));
  }

  // All our parameters, except two, can also be defined in the site alias.
  $setting_keys = array_diff_key($context['action']['parameters'], array('alias' => NULL, 'tag' => NULL));

  // Grab the keys in the context that's also defined in setting_keys.
  $settings = array_intersect_key($context, $setting_keys);
  // Remove those with false values. This allows alias settings and defaults to
  // override.
  $settings = array_filter($settings);

  // Add in any site alias settings.
  if (isset($site['deployotron'])) {
    $settings += $site['deployotron'];
  }

  // Add defaults.
  $settings += array(
    'restart-apache2' => FALSE,
    'restart-varnish' => FALSE,
    'no-dump' => FALSE,
    'dump-dir' => '/tmp',
    'no-updb' => FALSE,
  );

  $error = NULL;
  // Set site offline.
  if (!deployotron_invoke_process($site, 'variable-set', array('maintainance_mode', 1))) {
    return drake_action_error(dt('Error setting site offline.'));
  }
  // Dump database.
  if (!$settings['no-dump']) {
    $dump_name = $context['dump-dir'] . '/deploy-' . date('Y-m-d\TH:i:s') . '.sql';
    if (!deployotron_invoke_process($site, 'sql-dump', array(), array('no-ordered-dump' => TRUE, 'result-file' => $dump_name))) {
      $error = dt('Error dumping database.');
    }
  }

  // Run gittyup.
  if (!$error) {
    $options = array();
    if ($context['tag']) {
      $options['git-tag'] = $context['tag'];
    }
    if (!deployotron_invoke_process($site, 'gittyup', array(), $options)) {
      $error = dt('Error updating code base.');
    }
  }

  // Restart apache.
  if (!$error && $settings['restart-apache2']) {
    if (!deployotron_exec($site, 'sudo service apache2 restart')) {
      drush_log(dt('Error restarting apache2.'), 'error');
    }
  }

  // Run database updates.
  if (!$error && !$settings['no-updb']) {
    if (!deployotron_invoke_process($site, 'updb', array(), array('yes' => TRUE))) {
      $error = dt('Error running database updates.');
    }
  }

  // Clear cache.
  if (!$error && !$settings['no-cc-all']) {
    if (!deployotron_invoke_process($site, 'cc', array('all'), array())) {
      $error = dt('Error clearing cache.');
    }
  }

  // Set site online again.
  if (!deployotron_invoke_process($site, 'variable-set', array('maintainance_mode', 0), array())) {
    return drake_action_error(dt('Error setting site online'));
  }

  // Restart varnish.
  if (!$error && $settings['restart-varnish']) {
    if (!deployotron_exec($site, 'sudo service varnish restart')) {
      drush_log(dt('Error restarting varnish.'), 'error');
    }
  }

  // Return error if something failed.
  if ($error) {
    return drake_action_error($error);
  }

  drush_log(dt('Deployment complete.'), 'ok');
}

/**
 * Helper function for invoking drush command.
 */
function deployotron_invoke_process($site, $command, $args = array(), $options = array()) {
  $res = drush_invoke_process($site, $command, $args, $options, TRUE);
  if (!$res || $res['error_status'] != 0) {
    return FALSE;
  }
  return TRUE;
}

/**
 * Helper function for running shell command on site.
 */
function deployotron_exec($site, $command) {
  $exec = drush_shell_proc_build($site, $command, TRUE);
  // Return code 0 = success.
  if (drush_shell_proc_open($exec)) {
    return FALSE;
  }
  return TRUE;
}

/*

pre-deploy
on-deploy
post-deploy





site offline
db dump
gittyup
updb
cc all
site online

clear varnish





 */
