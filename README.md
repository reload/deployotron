Deployotron
===========

A deployment tool does not have to be complicated to set up or use. Deployotron is a Drush command to simplify deploying new code to a Drupal site.

[![Build Status](https://travis-ci.org/reload/deployotron.png?branch=master)](https://travis-ci.org/reload/deployotron)
[![Code Coverage](https://scrutinizer-ci.com/g/reload/deployotron/badges/coverage.png?s=0f0d54845fc1c45affcc0ad8c111e40f4e40c359)](https://scrutinizer-ci.com/g/reload/deployotron/)
[![Scrutinizer Quality Score](https://scrutinizer-ci.com/g/reload/deployotron/badges/quality-score.png?s=cd9fde12be1b74734b00d59618d4eb6c1bf5bfb0)](https://scrutinizer-ci.com/g/reload/deployotron/)


Getting started <a id="getting-started"></a>
===============

Get up and running with Deployotron in five easy steps:

1. **Commit your site to git** <br/> Deployotron expects your Drupal site to be committed to the root of your Git repository.
2. **Install Deployotron** <br/> Copy Deployotron into `sites/all/drush` and commit it. This ensures that everyone on your team is using the same version when deploying.
3. **Setup your aliases** <br/> Aliases makes Drush much more fun to use, and committing an `<sitename>aliases.drushrc.php` in `sites/all/drush` makes it easy to share them with the rest of the team. It is also the place to configure Deployotron.
4. **Clone the site on the server** <br/> Clone the site repository where you want to deploy it to. Rmember to use the same user as setup for the alias, to ensure that the permissions of the files are properly set up.
5. **Deploy!** <br/> Run `drush deploy @<alias>` from the root of your site. Deployotron will show you what actions will be executed as a part of the deployment and ask you to confirm before continuing them.
6. **(Oh no...)** <br/> Something bad happened? Run `drush omg @<alias>` to re-import the latest database dump and checkout the revision the site was at before deploying.

Overview <a id="overview"></a>
========

There are already a lot of ways to deploy a Drupal site - from rsync'ing the files to having Capistrano deploy the site when the build passes in Jenkins. Deployotron aims to be simple to use, but also usable as a part of a bigger setup.


In order to keep things simple Deployotron is working with a few assumptions:

* The source code for the site is in a git repository
* The root of the site is checked in in the root of the repository
* You can run Drush commands and git on the webserver your are deploying to 

Setup <a id="setup"></a>
=====

Install the Deplyotron Drush commands. We suggest you copy it into `sites/all/drush` folder and committed to the site repository. This ensures that everyone is running the exact same version of Deployotron when deploying.

If you do not have an `<sitename>.aliases.drushrc.php` file containing information about your environments then create one. We suggest keepin it in the `sites/all/drush/`  folder so everybody are using the same settings. 

Deployotron is configured for each alias by adding an array of options in the `'deployotron'` key of the alias array (see the example later, if that didn't make any sense).

All the double-dash options the deploy command takes can be specified this way, and it's recommended to at least define the `'branch'` option to select a default branch to deploy.

Initialize the environments by doing an initial `git clone` of the codebase in the destination directories.

Usage <a id="usage"></a>
=====

Deploying
---------

To run the deployment, use a command like:

    /var/www/site$ drush deploy @alias

To get a listing of all supported command line options, do a `drush help deploy`.

In order to be able to restart Apache2/Varnish, sudo needs to be set up to allow the deploying user to restart the services. See "Sudo setup" for details.

Example configuration:

    $aliases['staging'] = array(
      'remote-host' => 'example.com',
      'remote-user' => 'deploy_user',
      'uri' => 'default',
      'root' => '/path',
      'deployotron' => array(
        'branch' => 'develop',
        'dump-dir' => '/backups', // Defaults to /tmp.
        'num-dumps' => 3, // Defaults to 5. 0 for unlimited.
        'restart-apache2' => TRUE, // Defaults to FALSE.
      ),
    );

In addition to command line options you can add messages to be
displayed to the deploying user by using the following keys:


 * `message`: Shown at confirmation and after deployment.
 * `confirm_message`: Shown at confirmation.
 * `done_message`: Shown after deployment.
 * `confirm_message_<command>`: Shown at confirmation for the
   `<command>`.
 * `done_message_<command>`: Shown after deployment for the
   `<command>`.

These can be useful to remind the user of extra manual steps, or other
things they should be aware.

Recovering
----------

In case everything goes to hell after a deployment, you can do another deployment using a known good revision, or use:

    /var/www/site$ drush omg @<alias>

This will try to find recent database dumps, ask which to use and attempt to import the database and revert the codebase to the previous revision. It will not attempt to clear caches or restarting any services.

Help
----

Running `drush deployotron-actions` will give a full list of which commands uses which actions, and the options of all actions.

Sudo setup
==========

To give the deploying user (`remote-user` in the alias) permission to restart apache2/varnish, you need to configure sudo. Use the following command to edit a `sudoers.d` config file:

    sudo visudo -f /etc/sudoers.d/deployotron

And add the following to the file (replacing `deploy_user` with the
`remote-user` of the alias used for deployment):

    deploy_user          ALL=(root) NOPASSWD: /usr/sbin/service apache2 restart,/usr/sbin/service varnish restart
