<?php
/**
 * @file
 * Helper functions for Deployotron!
 */

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

/**
 * Get all available actions with callback and description.
 *
 * @param array $filter
 *   Which actions to accept. The order of the actions in this variable
 *   will determine the result.
 *
 * @return array
 *   Actions available (filtered and sorted).
 */
function deployotron_available_actions($filter = NULL) {
  $actions = array();
  $actions['site-offline'] = array(
    'description' => dt('Set site offline.'),
    // 'callback'    => 'deployotron_deploy_action_site_offline',
    // Use site-online as rollback.
    'rollback' => 'deployotron_deploy_action_site_online',
  );
  $actions['site-online'] = array(
    'description' => dt('Set site online.'),
    // 'callback'    => 'deployotron_deploy_action_site_online',
  );
  $actions['update-database'] = array(
    'description' => dt('Update database schema.'),
    // 'callback'    => 'deployotron_deploy_action_update_database',
  );
  $actions['clear-cache'] = array(
    'description' => dt('Clear cache.'),
    // 'callback'    => 'deployotron_deploy_action_clear_cache',
  );
  $actions['backup-database'] = array(
    'description' => dt('Dump database.'),
    // 'callback'    => 'deployotron_deploy_action_backup_database',
  );
  $actions['restore-database'] = array(
    'description' => dt('Restore database.'),
    // 'callback'    => 'deployotron_deploy_action_restore_database',
  );
  $actions['deploy-code'] = array(
    'description' => dt('Deploy code.'),
    // 'callback'    => 'deployotron_deploy_action_deploy_code',
  );
  $actions['restart-apache2'] = array(
    'description' => dt('Restart Apache2'),
    // 'callback'    => 'deployotron_deploy_action_restart_apache2',
  );
  $actions['restart-varnish'] = array(
    'description' => dt('Restart Varnish'),
    // 'callback'    => 'deployotron_deploy_action_restart_varnish',
  );
  $actions['sanity-check'] = array(
    'description' => dt('Sanity check'),
    // 'callback'    => 'deployotron_deploy_action_sanity_check',
  );

  foreach ($actions as $name => $action) {
    if (empty($action['callback'])) {
      $actions[$name]['callback'] = 'deployotron_deploy_action_' . strtr($name, array('-' => '_'));
    }
    if (empty($action['summary callback'])) {
      $actions[$name]['summary callback'] = 'deployotron_deploy_action_' . strtr($name, array('-' => '_')) . '_summary';
    }
  }

  if (is_array($filter)) {
    $filtered_actions = array();
    foreach ($filter as $action) {
      $filtered_actions[$action] = $actions[$action];
    }
    return $filtered_actions;
  }
  else {
    return $actions;
  }
}

/**
 * Determine which actions to run based on the settings.
 *
 * @param array $actions
 *   Actions consumer wants to run.
 * @param array $settings
 *   Settings for the actions.
 *
 * @return array
 *   Actions the consumer should run.
 */
function deployotron_determine_actions($actions, $settings) {
  $actions = deployotron_available_actions($actions);
  if (!$settings['restart-apache2']) {
    unset($actions['restart-apache2']);
  }
  if (!$settings['restart-varnish']) {
    unset($actions['restart-varnish']);
  }
  if ($settings['no-dump']) {
    unset($actions['backup-database']);
  }
  if ($settings['no-updb']) {
    unset($actions['update-database']);
  }
  if ($settings['no-cc-all']) {
    unset($actions['clear-cache']);
  }
  return $actions;
}

/**
 * Confirm the actions to run.
 *
 * Prompt for confirmation while showing the intended
 * actions to run.
 *
 * @param array $actions
 *   Actions to run.
 *
 * @return boolean
 *   Go or no-go.
 */
function deployotron_confirm_actions($actions, $settings) {
  $msg = "Deployotron wants to:\n\n";
  foreach ($actions as $action) {
    $msg .= $action['description'] . "\n";
  }
  $msg .= "\nusing settings:" . print_r($settings, TRUE) . "\n";
  $msg .= "\nIs this OK?";
  return drush_confirm($msg);
}

/**
 * Execute the actions.
 *
 * @param array $actions
 *   Actions to execute.
 * @param array $settings
 *   Settings for the actions.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_execute_actions($actions, $settings) {
  global $_deployotron_rollbacks, $_deployotron_site;
  $_deployotron_rollbacks = array();
  foreach ($actions as $action) {
    if (function_exists($action['callback'])) {
      $_deployotron_rollbacks[] = $action;
      if (!$action['callback']($_deployotron_site, $settings)) {
        return FALSE;
      }
      drush_log($action['description'], 'ok');
    }
    else {
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Execute rollbacks.
 *
 * @param array $actions
 *   Actions to execute.
 * @param array $settings
 *   Settings for the actions.
 *
 * @return boolean
 *   Success or failure.
 */
function deployotron_execute_rollbacks($rollbacks, $settings) {
  global $_deployotron_site;
  foreach (array_reverse($rollbacks) as $action) {
    if (isset($action['rollback']) && function_exists($action['rollback'])) {
      $action['rollback']($_deployotron_site, $settings);
    }
  }
  return TRUE;
}

/**
 * Determine SHA for this code base.
 *
 * @return string
 *   SHA of this code base.
 */
function deployotron_determine_sha($site, $settings) {
  $exec = drush_shell_proc_build($site, "git rev-parse HEAD", TRUE);
  // Return code 0 = success.
  if (drush_shell_exec($exec)) {
    return trim(drush_shell_exec_output());
  }
  return '';
}
