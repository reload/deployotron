Deployotron
===========

This is our back to basics deploy script.

Setup
=====

Copy the drakefile.php into sites/all/drush of the project, and run:

/var/www/site$ drush drake deployotron-install 

to install the dependencies (gittyup and drush_drake at the moment).

Create a `<sitename>.aliases.drushrc.php` file in the same directory,
with the definition of the different environments. Each environment
should have a `'git-tag'` key that defines the default branch/tag to
deploy. If using a branch, it should be prepended with origin to force
gittyup to make a detached HEAD checkout.

Initialize the environments by doing an initial git clone of the
codebase in the destination directories.

Usage
=====

Use:

    /var/www/site$ drush drake @alias deploy

To run the deployment.

The deploy task takes some options:

`<tag>`:
  Specific tag/branch to deploy.

`dump-dir=/path`:
  Where to store database dumps.

`no-dump=1`:
  Do not dump database.

`no-updb=1`:
  Do not run database updates.

`no-cc-all=1`:
  Do not clear caches.

Some options may also be defined on the alias:

    $aliases['staging'] = array(
      'parent' => '@common',
      'uri' => 'default',
      'root' => '/path',
      'git-tag' => 'origin/master',
      'deployotron' => array(
        'dump-dir' => '/backups',
        'no-dump' => FALSE,
        'no-updb' => FALSE,
        'no-cc-all' => FALSE,
        'restart-apache2' => TRUE,
        'restart-varnish' => TRUE,
      ),
    );
