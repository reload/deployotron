<?php
/**
 * @file
 * Drush commands for Deployotron!
 */

require_once 'deployotron.inc';
require_once 'deployotron.actions.inc';

/**
 * Implements hook_drush_command().
 */
function deployotron_drush_command() {
  $items['deployotron-deploy'] = array(
    'description' => 'Deploy.',
    'options' => array(
      'restart-apache2' => 'Restart Apache2.',
      'restart-varnish' => 'Restart Varnish.',
      'no-dump' => 'Do not create a database dump.',
      'dump-dir' => 'Directory to dump database into.',
      'no-updb' => 'Do not run update-db.',
      'no-cc-all' => 'Do not clear cache.',
    ),
    // 'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  return $items;
}

/**
 * Implements hook_drush_help().
 */
function deployotron_drush_help($section) {
  switch ($section) {
    case 'meta:drake:title':
      return dt('Drake');

    case 'meta:drake:summary':
      return dt('Do stuff.');

    case 'drush:drake':
      return dt("Does stuff...");

  }
}


/**
 * Action callback.
 *
 * Runs the deployment.
 */
function drush_deployotron_deploy() {
  // Determine settings.
  $settings = array(
    'restart-apache2' => drush_get_option('restart-apache2', FALSE),
    'restart-varnish' => drush_get_option('restart-varnish', FALSE),
    'no-dump' => drush_get_option('no-dump', FALSE),
    'dump-dir' => drush_get_option('dump-dir', '/tmp'),
    'no-updb' => drush_get_option('no-updb', FALSE),
    'no-cc-all' => drush_get_option('no-cc-all', FALSE),
  );

  $actions = deployotron_determine_actions(array(
    'sanity-check',
    'site-offline',
    'backup-database',
    'deploy-code',
    'update-database',
    'clear-cache',
    'restart-apache2',
    'site-online',
    'restart-varnish',
  ), $settings);

  if (!deployotron_confirm_actions($actions)) {
    return;
  }

  if (!deployotron_execute_actions($actions, $settings)) {
    return;
  }

  drush_print("DONE!");
}
