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
