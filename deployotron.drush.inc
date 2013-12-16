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
  $items['deploy'] = array(
    'description' => 'Deploy site to environment.',
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
    case 'meta:deployotron:title':
      return dt('Deployotron');

    case 'meta:deployotron:summary':
      return dt('Deploys site.');

    case 'drush:deploy':
      return dt("Deploy site to a specific environment.");

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

  if (!deployotron_confirm_actions($actions, $settings)) {
    return;
  }

  if (!deployotron_execute_actions($actions, $settings)) {
    return;
  }

  drush_print("Done.");
}
