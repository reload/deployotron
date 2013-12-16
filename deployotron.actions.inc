<?php
/**
 * @file
 * Actions for Deployotron!
 */

/**
 * Sanity check.
 *
 * Check for locally modified files and do a dry-run checkout.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_sanity_check($site, $settings) {
  $status = deployotron_exec($site, 'git status -s');
  if (!$status) {
    return drush_set_error(dt('Repository not clean.'));
  }

  return TRUE;
}

/**
 * Deploy code.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_deploy_code($site, $settings) {
  if (!deployotron_exec($site, 'git fetch')) {
    return drush_set_error(dt("Could not fetch from remote repository."));
  }

  if (!deployotron_exec($site, 'git checkout origin/' . $settings['branch'])) {
    return drush_set_error(dt('Could not checkout code.'));
  }

  return TRUE;
}

/**
 * Set site offline.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_site_offline($site, $settings) {
  if (!deployotron_invoke_process($site, 'variable-set', array('maintainance_mode', 1))) {
    return drush_set_error(dt('Error setting site offline.'));
  }
  return TRUE;
}

/**
 * Set site offline.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_site_online($site, $settings) {
  if (!deployotron_invoke_process($site, 'variable-set', array('maintainance_mode', 0))) {
    return drush_set_error(dt('Error setting site online.'));
  }
  return TRUE;
}

/**
 * Backup database.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_backup_database($site, $settings) {
  $date = date('Y-m-d\TH:i:s');
  $sha = deployotron_determine_sha($site, $settings);
  $dump_name = $settings['dump-dir'] . "/deploy.$date.$sha.sql";
  if (!deployotron_invoke_process($site, 'sql-dump', array(), array('no-ordered-dump' => TRUE, 'result-file' => $dump_name))) {
    return drush_set_error('Error dumping database.');
  }
  return TRUE;
}

/**
 * Restart Apache2.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_restart_apache2($site, $settings) {
  if (!deployotron_exec($site, 'sudo service apache2 restart')) {
    return drush_set_error(dt('Error restarting apache2.'), 'error');
  }
  return TRUE;
}

/**
 * Restart Varnish.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_restart_varnish($site, $settings) {
  if (!deployotron_exec($site, 'sudo service varnish restart')) {
    return drush_set_error(dt('Error restarting varnish.'), 'error');
  }
  return TRUE;
}

/**
 * Update database.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_update_database($site, $settings) {
  if (!deployotron_invoke_process($site, 'updb', array(), array('yes' => TRUE))) {
    return drush_set_error(dt('Error running database updates.'));
  }
  return TRUE;
}

/**
 * Clear cache.
 *
 * @param array $settings
 *   Settings.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_deploy_action_clear_cache($site, $settings) {
  if (!deployotron_invoke_process($site, 'cc', array('all'), array())) {
    return drush_set_error(dt('Error clearing cache.'));
  }
  return TRUE;
}
