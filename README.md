Deployotron
===========

This is our back to basics deploy script.

[![Build Status](https://travis-ci.org/reload/deployotron.png?branch=master)](https://travis-ci.org/reload/deployotron)

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
