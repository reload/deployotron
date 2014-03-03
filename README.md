Deployotron
===========

Deployotron is a Drush command to simplify deploying new code to a
Drupal site.

There's already a lot of ways to deploy ones Drupal site, from FTPing
up the files to having Capistrano deploy the site when the build
passes in Jenkins. Deployotron aims to be simple to use, but also
usable as a part of a bigger setup.

[![Build Status](https://travis-ci.org/reload/deployotron.png?branch=master)](https://travis-ci.org/reload/deployotron)
[![Code Coverage](https://scrutinizer-ci.com/g/reload/deployotron/badges/coverage.png?s=0f0d54845fc1c45affcc0ad8c111e40f4e40c359)](https://scrutinizer-ci.com/g/reload/deployotron/)
[![Scrutinizer Quality Score](https://scrutinizer-ci.com/g/reload/deployotron/badges/quality-score.png?s=cd9fde12be1b74734b00d59618d4eb6c1bf5bfb0)](https://scrutinizer-ci.com/g/reload/deployotron/)

Overview
========

In order to keep things simple, we're working with a few assumptions:

That the code is in GIT, and that the root of the site is checked in.

That you can run Drush commands and GIT on the live webserver and the
root of the site on the webserver is a git checkout, and

That you've set up Drush aliases to reach the live webserver.

For everyone's sanity, we suggest having a Drush alias file in
sites/all/drush/<short-site-alias>.aliases.drushrc.php that defines
relevant environments (production, dev, etc.), so that everybody is
using the same settings.

Setup
=====

Clone Deployotron into sites/all/drush.

Create a `<sitename>.aliases.drushrc.php` file in the same directory,
with the definition of the different environments.

Deployotron is configured for each alias by adding an array of options
in the `'deployotron'` key of the alias array (see the example later,
if that didn't make any sense). All the double-dash options the deploy
command takes can be specified this way, and it's recommended to at
least define the `'branch'` option to select a default branch to
deploy.

Initialize the environments by doing an initial git clone of the
codebase in the destination directories.

Usage
=====

Deploying
---------

To run the deployment, use a command like:

    /var/www/site$ drush deploy @alias

To get a listing of all supported command line options, do a `drush
help deploy`.

In order to be able to restart Apache2/Varnish, sudo needs to be set
up to allow the deploying user to restart the services. See "Sudo
setup" for details.

Example configuration:

    $aliases['staging'] = array(
      'remote-host' => 'example.com',
      'remote-user' => 'deploy_user',
      'uri' => 'default',
      'root' => '/path',
      'deployotron' => array(
        'branch' => 'develop',
        'dump-dir' => '/backups',
        'restart-apache2' => TRUE,
      ),
    );


Recovering
----------

In case everything goes to hell after a deployment, you can do another
deployment using a known good revision, or use:

    /var/www/site$ drush omg @alias

This will try to find recent database dumps, ask which to use and
attempt to import the database and revert the codebase to the previous
revision. It will not attempt to clear caches or restarting any
services.

Sudo setup
==========

To give the deploying user (`remote-user` in the alias) permission to
restart apache2/varnish, you need to configure sudo. Use the following
command to edit a sudoers.d config file:

    sudo visudo -f /etc/sudoers.d/deployotron

And add the following to the file (replacing `deploy_user` with the
`remote-user` of the alias used for deployment):

    deploy_user          ALL=(root) NOPASSWD: /usr/sbin/service apache2 restart,/usr/sbin/service varnish restart
