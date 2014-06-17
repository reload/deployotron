<?php

/**
 * @file
 * PHPUnit Tests for Deployotron.
 */

define('DEPLOYOTRON_SITE_REPO', 'https://github.com/reload/deployotron_testsite.git');

/**
 * Deployotron testing class.
 */
class DeployotronCase extends Drush_CommandTestCase {

  /**
   * Token for a testing Flowdock flow.
   */
  protected $flowdockToken = 'cf5e1bd1f986989265fdcc5f92af31dd';
  /**
   * Setup before each test case.
   */
  public function setUp() {
    // Deployotron needs a site to run in.
    if (file_exists($this->webroot())) {
      exec("rm -rf " . $this->webroot() . '/sites/*');
    }
    $this->setUpDrupal(1, TRUE);

    // For speed, cache the repo we clone.
    $cached_repo = $this->cachedRepo();
    if (!file_exists($cached_repo)) {
      exec("cd " . dirname($cached_repo) . " && git clone --bare " . DEPLOYOTRON_SITE_REPO . " " . basename($cached_repo), $output, $rc);
      if ($rc != 0) {
        $this->fail('Problem cloning site for deployment.');
      }
    }

    // And a 'site' to deploy to.
    $deploy_site = $this->deploySite();
    if (file_exists($deploy_site)) {
      // Start afresh.
      `rm -rf $deploy_site`;
    }
    exec("cd " . dirname($deploy_site) . " && git clone " . $cached_repo . " $deploy_site", $output, $rc);
    if ($rc != 0) {
      $this->fail('Problem cloning site for deployment.');
    }
    // Check out a known version.
    exec("cd " . $deploy_site . " && git checkout 7a9166ac76bb63f45a5a8a6b9a4e3a58eb04da6e");

    // Pretend that the current site is a clone of the same. Local git commands
    // need this.
    exec('cp -r ' . $deploy_site . '/.git ' . $this->webroot());

    // We'll need to bind them together with an alias file in the 'local' site.
    $site_drush = $this->webroot() . '/sites/all/drush';

    // Leech on the settings file of the site Drush installed. We need one for
    // the DB setup for the deployment site, and we're not using the 'local'
    // one.
    copy($this->webroot() . '/sites/dev/settings.php', $deploy_site . '/sites/default/settings.php');
    if (file_exists($site_drush)) {
      // Start afresh.
      `rm -rf $site_drush`;
    }
    mkdir($site_drush, 0777, TRUE);

    // Create or clear the dump dir.
    $dump_dir = $this->dumpPath();
    if (file_exists($dump_dir)) {
      `rm -rf $dump_dir`;
    }
    mkdir($dump_dir);

    $this->writeAlias(array(
        'branch' => 'master',
      ));

    // Finally, we need to install the deployotron drush command.
    mkdir($site_drush . '/deployotron', 0777, TRUE);
    $deployotron_dir = dirname(__DIR__);
    // Symlinking files in, in order to play nice with test coverage
    // reporting.
    symlink($deployotron_dir . '/deployotron.drush.inc', $site_drush . '/deployotron/deployotron.drush.inc');
    symlink($deployotron_dir . '/deployotron.actions.inc', $site_drush . '/deployotron/deployotron.actions.inc');
  }

  /**
   * Create the deployotron alias with the given options.
   */
  public function writeAlias($deploy_options) {
    $deploy_options += array(
      'dump-dir' => $this->dumpPath(),
    );
    $aliases = array(
      'uri' => 'default',
      'root' => $this->deploySite(),
      'deployotron' => $deploy_options,
    );
    $alias_content = "<?php
\$aliases['deployotron'] = " . var_export($aliases, TRUE) . ';';
    $site_drush = $this->webroot() . '/sites/all/drush';
    file_put_contents($site_drush . '/deployotron.aliases.drushrc.php', $alias_content);
  }

  /**
   * Path to the cached repo.
   */
  public function cachedRepo() {
    return $this->directory_cache('deployotron_drupal_repo');
  }

  /**
   * Path to the faked remote site.
   */
  public function deploySite() {
    return UNISH_SANDBOX . '/deployotron';
  }

  /**
   * Path to where we put the dumps.
   */
  public function dumpPath() {
    return UNISH_SANDBOX . '/deployotron_dumps';
  }

  /**
   * Get a list of files in a directory.
   */
  public function fileList($dir) {
    exec('ls ' . escapeshellarg($dir), $output, $rc);
    if ($rc === 0) {
      return $output;
    }
    return array();
  }

  /**
   * Test help commands.
   */
  public function testHelp() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // Check that the help is available.
    $this->drush('help', array('deploy'), array(), NULL, $this->webroot());
    $this->assertRegExp('/Deploy site to a specific environment/', $this->getOutput());

    // Also for the omg command.
    $this->drush('help', array('omg'), array(), NULL, $this->webroot());
    $this->assertRegExp('/Try to find a backup, and restore it to the site/', $this->getOutput());

    // And check that the topic command outputs something.
    $this->drush('topic', array('deployotron-actions'), array(), NULL, $this->webroot());
    // Check some random strings.
    $this->assertRegExp('/Commands\\n--------/', $this->getOutput());
    $this->assertRegExp('/Actions\\n-------/', $this->getOutput());
    $this->assertRegExp('/deploy runs the actions: SanityCheck/', $this->getOutput());
    $this->assertRegExp('/DeployCode:\\nChecks out a specified/', $this->getOutput());
    $this->assertRegExp('/--branch/', $this->getOutput());

    // Use the filter arg to get full coverage of hook_drush_help().
    $this->drush('help', array(), array('filter' => NULL, 'n' => NULL), NULL, $this->webroot());
    $this->assertRegExp('/Deployotron: Deploys site./', $this->getOutput());
  }

  /**
   * Test for command line argument errors.
   */
  public function testErrors() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // No alias error message.
    $this->drush('deploy 2>&1', array(), array('y' => TRUE, 'branch' => '', 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/No alias given/', $this->getOutput());

    // Bad alias error.
    $this->drush('deploy 2>&1', array('@badalias'), array('y' => TRUE, 'branch' => '', 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Invalid alias/', $this->getOutput());

    // Also check that aborting works.
    $this->drush('deploy 2>&1', array('@deployotron'), array('n' => TRUE, 'branch' => '', 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot());
    $this->assertRegExp('/Aborting/', $this->getOutput());
    $this->assertNotRegExp('/Done/', $this->getOutput());
  }

  /**
   * Check that the basics work.
   */
  public function testBasic() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // Check that deployment works.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot());
    // Also check that we get a notice for for having both branch (on alias) and
    // sha (command line) specified.
    $this->assertRegExp('/More than one of branch\\/tag\\/sha specified, using sha./', $this->getOutput());
    $this->assertRegExp('/HEAD now at 04256b5992d8b4a4fae25c7cb7888583749fabc0/', $this->getOutput());

    // Check that VERSION.txt was created.
    $this->assertFileExists($this->deploySite() . '/VERSION.txt');

    // Check that a invalid tag/branch prints the proper error message.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'tag' => 'slipstream', 'sha' => ''), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Error finding SHA for tag/', $this->getOutput());

    // Check that a invalid SHA prints the proper error message.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'tag' => '', 'sha' => 'deadc0de'), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Unknown SHA/', $this->getOutput());

    // Check that missing branch/tag/sha prints the proper error message.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'tag' => '', 'sha' => ''), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/You must provide at least one of --branch, --tag or --sha/', $this->getOutput());

    // Check that a dirty checkout makes deployment fail..
    file_put_contents($this->deploySite() . '/index.php', 'stuff');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Repository not clean/', $this->getOutput());

    // Check that a dirty checkout makes deployment fail, even if added to git.
    exec('cd ' . $this->deploySite() . ' && git add -u');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Uncommitted changes in the index/', $this->getOutput());

    // Fix the the checkout and check that we can now deploy, and throw in
    // no-confirm for coverage.
    exec('cd ' . $this->deploySite() . ' && git reset --hard');
    $this->drush('deploy 2>&1', array('@deployotron'), array('no-confirm' => TRUE, 'branch' => '', 'sha' => 'b9471948c3f83a665dd4f106aba3de8962d69b42'), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at b9471948c3f83a665dd4f106aba3de8962d69b42/', $this->getOutput());

    // VERSION.txt should still exist.
    $this->assertFileExists($this->deploySite() . '/VERSION.txt');
    // Check content.
    $version_txt = file_get_contents($this->deploySite() . '/VERSION.txt');
    $this->assertRegExp('/Deployment info/', $version_txt);
    $this->assertRegExp('/SHA: b9471948c3f83a665dd4f106aba3de8962d69b42/', $version_txt);
    $this->assertRegExp('/Time of deployment: /', $version_txt);
    $this->assertRegExp('/Deployer: /', $version_txt);

    // Check that the switch for echo didn't got written to the file.
    $this->assertNotRegExp('/-e/', $version_txt);

    // Check that a file in the way of a new file will cause the deployment to
    // roll back.
    file_put_contents($this->deploySite() . '/sites/all/modules/coffee', 'stuff');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Aborting/', $this->getOutput());
    $this->assertRegExp('/Could not checkout code/', $this->getOutput());

    // Check that we rolled back.
    $this->assertRegExp('/Rolled back set site offline/', $this->getOutput());

    // Check that we're still at the same version.
    exec('cd ' . $this->deploySite() . ' && git rev-parse HEAD', $output);
    $this->assertEquals('b9471948c3f83a665dd4f106aba3de8962d69b42', trim(implode('', $output)));

    // Remove the blocker and try again.
    unlink($this->deploySite() . '/sites/all/modules/coffee');
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at fbcaa29d45716edcbedc3c325bfbab828f1ce838/', $this->getOutput());
    $version_txt = file_get_contents($this->deploySite() . '/VERSION.txt');
    $this->assertRegExp('/Branch: master/', $version_txt);
    $this->assertRegExp('/SHA: fbcaa29d45716edcbedc3c325bfbab828f1ce838/', $version_txt);
  }

  /**
   * Test configured messages.
   */
  public function testMessages() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());
    $this->writeAlias(array(
        'branch' => 'master',
        'no-updb' => TRUE,
        'no-cc-all' => TRUE,
        'no-offline' => TRUE,
        'no-dump' => TRUE,
        'message' => 'Those about to deploy, salute you',
      ));

    // Check that messages are printed.
    $options = array(
      'y' => TRUE,
      'branch' => '',
      'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0',
    );
    $this->drush('deploy 2>&1', array('@deployotron'), $options, NULL, $this->webroot());
    $this->assertRegExp('/Those about to deploy, salute you/', $this->getOutput());

    $this->writeAlias(array(
        'branch' => 'master',
        'no-updb' => TRUE,
        'no-cc-all' => TRUE,
        'no-offline' => TRUE,
        'no-dump' => TRUE,
        'confirm_message' => 'Those about to deploy, salute you',
        'done_message' => "Are you pondering what I'm pondering?",
      ));

    // Check that confirm message is printed if aborting, but not done message.
    $options = array(
      'n' => TRUE,
      'branch' => '',
      'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0',
    );
    $this->drush('deploy 2>&1', array('@deployotron'), $options, NULL, $this->webroot());
    $this->assertRegExp('/Those about to deploy, salute you/', $this->getOutput());
    $this->assertNotRegExp("/Are you pondering what I'm pondering/", $this->getOutput());

    // Check that both are printed on successfull deploy.
    $options = array(
      'y' => TRUE,
      'branch' => '',
      'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0',
    );
    $this->drush('deploy 2>&1', array('@deployotron'), $options, NULL, $this->webroot());
    $this->assertRegExp('/Those about to deploy, salute you/', $this->getOutput());
    $this->assertRegExp("/Are you pondering what I'm pondering/", $this->getOutput());
  }

  /**
   * Test the omg command.
   */
  public function testOmg() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // Start with a simple deployment.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'sha' => '04256b5992d8b4a4fae25c7cb7888583749fabc0'), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at 04256b5992d8b4a4fae25c7cb7888583749fabc0/', $this->getOutput());

    // Set a variable.
    $this->drush('vset', array('magic_variable', 'bumblebee'), array(), '@deployotron', $this->webroot());
    // Check that we can see the value.
    $this->drush('vget', array('magic_variable'), array(), '@deployotron', $this->webroot());
    $this->assertRegExp("/magic_variable: .bumblebee./", $this->getOutput());

    // Deploy another version.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'sha' => 'b9471948c3f83a665dd4f106aba3de8962d69b42'), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at b9471948c3f83a665dd4f106aba3de8962d69b42/', $this->getOutput());

    // And a third time.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at fbcaa29d45716edcbedc3c325bfbab828f1ce838/', $this->getOutput());

    // Now check the rollback.
    $this->drush('omg 2>&1', array('@deployotron'), array('y' => TRUE, 'choice' => 3), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at 7a9166ac76bb63f45a5a8a6b9a4e3a58eb04da6e/', $this->getOutput());

    // Check that the variable has disappeared.
    $this->drush('vget', array('magic_variable'), array(), '@deployotron', $this->webroot(), self::EXIT_ERROR);
    $this->assertNotRegExp("/magic_variable: .bumblebee./", $this->getOutput());
  }

  /**
   * Test Flowdock integration.
   *
   * No attempt at verifying that the message was shown is done, but we assume
   * that Flowdock validates the message and returns something other than
   * success if it finds anything wrong with it, which should cause Deployotron
   * to print an error.
   */
  public function testFlowdock() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // Run a deployment with Flowdock notification.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'flowdock-token' => $this->flowdockToken), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at fbcaa29d45716edcbedc3c325bfbab828f1ce838/', $this->getOutput());

    // Check that we attempted to notify Flowdock.
    $this->assertRegExp('/Sending Flowdock notification/', $this->getOutput());
    // Check for error message.
    $this->assertNotRegExp('/Unexpected response from Flowdock/', $this->getOutput());

    // Test for generic error messages.
    $this->assertNotRegExp('/\[error\]/', $this->getOutput());

    // Try again with an invalid token.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'flowdock-token' => 'badc0de'), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at fbcaa29d45716edcbedc3c325bfbab828f1ce838/', $this->getOutput());

    // Check that we attempted to notify Flowdock.
    $this->assertRegExp('/Sending Flowdock notification/', $this->getOutput());
    // Check for error message.
    $this->assertRegExp('/Unexpected response from Flowdock/', $this->getOutput());

    // Test for generic error messages.
    $this->assertNotRegExp('/\[error\]/', $this->getOutput());
  }

  /**
   * Test that overriding Apache2 and Varnish commands work.
   *
   * Tests the actions as a side-effect.
   */
  public function testCommandOverride() {
    $this->writeAlias(array(
        'branch' => 'master',
        'restart-apache2' => TRUE,
        'restart-apache2-command' => '/bin/true',
        'restart-varnish' => TRUE,
        'restart-varnish-command' => '/bin/true',
      ));

    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // Check that deployment works.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at fbcaa29d45716edcbedc3c325bfbab828f1ce838/', $this->getOutput());

    // Check that failing Apache2 restart command gets caught.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'restart-apache2-command' => '/bin/false'), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Error restarting apache2/', $this->getOutput());

    // Check that failing Varnish restart command gets caught.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'restart-varnish-command' => '/bin/false'), NULL, $this->webroot(), self::EXIT_ERROR);
    $this->assertRegExp('/Error restarting varnish/', $this->getOutput());
  }

  /**
   * Test no-deploy.
   *
   * And that Flowdock and VERSION.txt prints the appropriate message.
   */
  public function testNoDeploy() {
    $this->writeAlias(array(
        'branch' => 'master',
      ));
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    // Check that deployment works.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at fbcaa29d45716edcbedc3c325bfbab828f1ce838/', $this->getOutput());

    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'no-deploy' => TRUE, 'flowdock-token' => $this->flowdockToken), NULL, $this->webroot());
    // Check VERSION.txt message.
    $this->assertRegExp('/No version deployed, not creating\/updating VERSION.txt/', $this->getOutput());

    // Check Flowdock message.
    $this->assertRegExp('/No version deployed, not sending Flowdock notification/', $this->getOutput());
  }

  /**
   * Test dump file purging.
   */
  public function testDumpFilePurging() {
    // Drush 5 needs to be kicked to see the new command.
    $this->drush('cc', array('drush'), array(), NULL, $this->webroot());

    $expected_num_dumps = 0;
    // Check that the dumpdir is empty.
    $this->assertCount($expected_num_dumps, $this->fileList($this->dumpPath()));

    $shas = array(
      '04256b5992d8b4a4fae25c7cb7888583749fabc0',
      'b9471948c3f83a665dd4f106aba3de8962d69b42',
      'fbcaa29d45716edcbedc3c325bfbab828f1ce838',
    );

    // Create a bunch of dumps.
    foreach (range(1, 2) as $num) {
      foreach ($shas as $sha) {
        $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'sha' => $sha), NULL, $this->webroot());
        $this->assertRegExp('/HEAD now at ' . $sha . '/', $this->getOutput());
        $expected_num_dumps++;
        $this->assertCount($expected_num_dumps, $this->fileList($this->dumpPath()));
      }
    }

    $dumps = $this->fileList($this->dumpPath());
    sort($dumps);
    $expected_dumps = array_slice($dumps, -5, 5);

    // Now limit the amount of dumps and check the number.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'sha' => $sha, 'num-dumps' => 5), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at ' . $sha . '/', $this->getOutput());
    $this->assertCount(5, $this->fileList($this->dumpPath()));
    $dumps = $this->fileList($this->dumpPath());
    sort($dumps);
    $this->assertEqual($expected_dumps, $dumps);

    $expected_dumps = array_slice($dumps, -3, 3);

    // And again.
    $this->drush('deploy 2>&1', array('@deployotron'), array('y' => TRUE, 'branch' => '', 'sha' => $sha, 'num-dumps' => 3), NULL, $this->webroot());
    $this->assertRegExp('/HEAD now at ' . $sha . '/', $this->getOutput());
    $this->assertCount(5, $this->fileList($this->dumpPath()));
    $dumps = $this->fileList($this->dumpPath());
    sort($dumps);
    $this->assertEqual($expected_dumps, $dumps);
  }
}
