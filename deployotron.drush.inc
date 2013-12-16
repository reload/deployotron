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
    'arguments' => array(
      'site-alias' => 'Site alias to deploy to.'
    ),
    'options' => array(
      'branch' => 'Branch to check out.',
      'restart-apache2' => 'Restart Apache2.',
      'restart-varnish' => 'Restart Varnish.',
      'no-dump' => 'Do not create a database dump.',
      'dump-dir' => 'Directory to dump database into.',
      'no-updb' => 'Do not run update-db.',
      'no-cc-all' => 'Do not clear cache.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_ROOT,
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
 * Verify arguments to the deploy command.
 */
function drush_deployotron_deploy_validate($site = NULL) {
  // We'd prefer to use something like drush_set_context, but it claims it's for
  // private use, so we'll have to fall back to good old global vars.
  global $_deployotron_site;

  if (empty($site)) {
    return drush_set_error('NO_ALIAS', dt('No alias given.'));

  }
  $_deployotron_site = drush_sitealias_get_record($site);
  if (empty($_deployotron_site)) {
    return drush_set_error('BAD_ALIAS', dt('Invalid alias.'));
  }
}

/**
 * Action callback.
 *
 * Runs the deployment.
 */
function drush_deployotron_deploy($site) {
  global $_deployotron_settings, $_deployotron_site;
  // Determine settings.
  $settings = array(
    'branch' => drush_get_option('branch', NULL),
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

  $_deployotron_settings = $settings;

  if (!deployotron_confirm_actions($actions, $settings)) {
    return drush_user_abort();
  }

  if (!deployotron_execute_actions($actions, $settings)) {
    return FALSE;
  }

  drush_print("Done.");
}

/**
 * Rollback deploy command.
 */
function drush_deployotron_deploy_rollback() {
  global $_deployotron_settings, $_deployotron_rollbacks;
  if (count($_deployotron_rollbacks)) {
    deployotron_execute_rollbacks($_deployotron_rollbacks, $settings);
  }
}
