<?php

/**
 * @file
 * Drush commands for Deployotron!
 */

define('DEPLOYOTRON_VERSION', '2.1.0');

require_once 'deployotron.actions.inc';

use Deployotron\ActionFactory;

/**
 * Implements hook_drush_command().
 */
function deployotron_drush_command() {
  $items['deploy'] = array(
    'description' => 'Deploy site to environment.',
    'command-hook' => 'deployotron_run',
    'callback arguments' => array('deploy'),
    'arguments' => array(
      'site-alias' => 'Site alias to deploy to.',
    ),
    'options' => array(
      'no-confirm' => 'Do not require confirmation before running.',
    ),
    'sub-options' => array(),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
  );
  foreach (ActionFactory::getActions('deploy') as $action) {
    if ($options = $action->getOptions()) {
      $items['deploy']['options'] += $options['options'];
      foreach ($options['sub-options'] as $sub => $sub_options) {
        if (!isset($items['deploy']['sub-options'][$sub])) {
          $items['deploy']['sub-options'][$sub] = array();
        }
        $items['deploy']['sub-options'][$sub] += $sub_options;
      }
    }
  }

  $items['oh-my-god'] = array(
    'description' => 'Try to restore a site from a backup.',
    'command-hook' => 'deployotron_run',
    'callback arguments' => array('omg'),
    'arguments' => array(
      'site-alias' => 'Site alias to try and save.',
    ),
    'options' => array(
      'no-confirm' => 'Do not require confirmation before running.',
    ),
    'sub-options' => array(),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
    'aliases' => array(
      'omg',
    ),
  );
  foreach (ActionFactory::getActions('omg') as $action) {
    if ($options = $action->getOptions()) {
      $items['oh-my-god']['options'] += $options['options'];
      foreach ($options['sub-options'] as $sub => $sub_options) {
        if (!isset($items['oh-my-god']['sub-options'][$sub])) {
          $items['oh-my-god']['sub-options'][$sub] = array();
        }
        $items['oh-my-god']['sub-options'][$sub] += $sub_options;
      }
    }
  }

  $items['deployotron-actions'] = array(
    'description' => 'Display the actions run by deployotron.',
    'hidden' => TRUE,
    'topic' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'callback' => 'deployotron_print_actions',
  );

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

    case 'drush:oh-my-god':
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
      drush_set_default($option, $value);
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
function drush_deployotron_run($name, $site_alias) {
  global $_deployotron_actions, $_deployotron_rollbacks;
  $_deployotron_rollbacks = array();

  $message = "About to:\n\n";
  $i = 1;
  foreach ($_deployotron_actions as $action) {
    if ($action->enabled()) {
      $message .= ($i++) . ': ' . strtr($action->getDescription(), array("\n" => "\n   ")) . "\n\n";
    }
  }

  $confirm_message = trim(drush_get_option('message', NULL) . "\n\n" .
                     drush_get_option('confirm_message', NULL) . "\n\n" .
                     drush_get_option('confirm_message_' . $name, NULL));
  if (!empty($confirm_message)) {
    $message .= $confirm_message . "\n\n";
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
  // Using an ArrayObject instead of a plain array, as arrays are passed by copy
  // and objects aren't. Relying on the callee to remember the & is error-prone.
  $state = new ArrayObject();
  foreach ($_deployotron_actions as $action) {
    if ($action->enabled()) {
      $short = $action->getShort();
      drush_log($action->getRunMessage(), 'status');
      $success = $action->run($state);
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

  $done_message = trim(drush_get_option('message', NULL) . "\n\n" .
                  drush_get_option('done_message', NULL) . "\n\n" .
                  drush_get_option('done_message_' . $name, NULL));
  if (!empty($done_message)) {
    drush_print("\n\n" . $done_message);
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

/**
 * Callback for topic depoloyotron-actions.
 */
function deployotron_print_actions() {
  ActionFactory::getHelp();
}
