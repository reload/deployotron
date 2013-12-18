<?php
/**
 * @file
 * Drush commands for Deployotron!
 *
 * In order to be able to restart apache2/varnish, use the following command to
 * edit a sudoers.d config file:
 *
 * sudo visudo -f /etc/sudoers.d/deployotron
 *
 * And add the following to the file:
 *
 * deploy_user          ALL=(root) NOPASSWD: /usr/sbin/service apache2 restart,/usr/sbin/service varnish restart
 */

require_once 'deployotron.actions.inc';

use Deployotron\ActionFactory;

/**
 * Implements hook_drush_command().
 */
function deployotron_drush_command() {
  $items['deploy'] = array(
    'description' => 'Deploy site to environment.',
    'callback' => 'drush_deployotron_run',
    'callback arguments' => array('deploy'),
    'arguments' => array(
      'site-alias' => 'Site alias to deploy to.',
    ),
    'options' => array(
      'no-confirm' => 'Do not require confirmation before running.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
  );
  foreach (ActionFactory::getActions('deploy') as $action) {
    if ($options = $action->getOptions()) {
      $items['deploy']['options'] += $options;
    }
  }

  $items['oh-my-god'] = array(
    'description' => 'Try to restore a site from a backup.',
    'callback' => 'drush_deployotron_run',
    'callback arguments' => array('omg'),
    'arguments' => array(
      'site-alias' => 'Site alias to try and save.',
    ),
    'options' => array(
      'no-confirm' => 'Do not require confirmation before running.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => array(
      'omg',
    ),
  );
  foreach (ActionFactory::getActions('omg') as $action) {
    if ($options = $action->getOptions()) {
      $items['oh-my-god']['options'] += $options;
    }
  }

  return $items;
}

/**
 * Implements hook_drush_help().
 */
function deployotron_drush_help($section) {
  switch ($section) {
    case 'meta:deployotron:title':
      return dt('Deployotron');

    case 'meta:deployotron:summary':
      return dt('Deploys site.');

    case 'drush:deploy':
      return dt("Deploy site to a specific environment.");

    case 'drush:omg':
      return dt("Try to find a backup, and restore it to the site.");

  }
}

/**
 * Verify arguments to the command.
 */
function drush_deployotron_run_validate($name, $site_alias = NULL) {
  // We'd prefer to use something like drush_set_context, but it claims it's for
  // private use, so we'll have to fall back to good old global vars.
  global $_deployotron_actions;

  if (empty($site_alias)) {
    return drush_set_error('NO_ALIAS', dt('No alias given.'));

  }
  $site = drush_sitealias_get_record($site_alias);
  if (empty($site)) {
    return drush_set_error('BAD_ALIAS', dt('Invalid alias.'));
  }

  // Copy options from the deployotron key in as command line options.
  if (!empty($site['deployotron'])) {
    foreach ($site['deployotron'] as $option => $value) {
      drush_set_option($option, $value);
    }
  }

  $_deployotron_actions = ActionFactory::getActions($name, $site);
  foreach ($_deployotron_actions as $action) {
    if ($action->enabled()) {
      if (!$action->validate()) {
        if (!drush_get_error()) {
          drush_set_error(dt('@action validation failed without a message.', array('@action' => $action->getShort())));
        }
        return FALSE;
      }
    }
  }
}

/**
 * Command callback.
 */
function drush_deployotron_run($name, $site) {
  global $_deployotron_actions, $_deployotron_rollbacks;
  $_deployotron_rollbacks = array();

  $message = "About to:\n\n";
  $i = 1;
  foreach ($_deployotron_actions as $action) {
    if ($action->enabled()) {
      $message .= ($i++) . ': ' . strtr($action->describe(), array("\n" => "\n   ")) . "\n\n";
    }
  }

  if (drush_get_option('no-confirm', FALSE)) {
    drush_print($message);
  }
  else {
    $message .= "\nContinue?";
    if (!drush_confirm($message)) {
      return drush_user_abort();
    }
  }

  $success = FALSE;
  foreach ($_deployotron_actions as $action) {
    if ($action->enabled()) {
      $short = $action->getShort();
      drush_log($action->getRunMessage(), 'status');
      $success = $action->run();
      $_deployotron_rollbacks[] = $action;
      if (!$success) {
        drush_log(dt('@action failed.', array('@action' => $short)), 'error');
        drush_print();
        return FALSE;
      }
      else {
        drush_log('', 'ok');
        drush_print();
      }
    }
  }

  drush_print("Done.");
}

/**
 * Rollback deploy command.
 */
function drush_deployotron_run_rollback($name) {
  global $_deployotron_rollbacks;

  foreach ($_deployotron_rollbacks as $action) {
    if ($action->enabled()) {
      if ($action->rollback() !== 'no rollback') {
        drush_log(dt('Rolled back @action.', array('@action' => $action->getShort())), 'ok');
      }
    }
  }
}
