<?php

/**
 * @file
 * Unit tests for the Action class.
 */

require_once dirname(__DIR__) . '/deployotron.actions.inc';

/**
 * Log function for drush_log that passes messages to test class.
 */
function deployotron_log_proxy($entry = NULL) {
  static $test_class = NULL;
  if ($entry instanceof DeployotronActionCase) {
    $test_class = $entry;
    return;
  }
  elseif (isset($test_class)) {
    $test_class->drushLog($entry);
  }
}

/**
 * Deployotron testing class.
 */
class DeployotronActionCase extends Drush_UnitTestCase {
  protected $log;
  protected $prevLog;

  /**
   * Setup before each test.
   */
  public function setup() {
    $this->log = array();
    // Set ourselves as the log receiver.
    deployotron_log_proxy($this);
    // Save previous value.
    $this->prevLog = drush_get_context('DRUSH_LOG_CALLBACK', NULL);
    // Hook into drush_log.
    drush_set_context('DRUSH_LOG_CALLBACK', 'deployotron_log_proxy');
  }

  /**
   * Teardown after each test.
   */
  public function tearDown() {
    // Reset drush_log for good me assure.
    drush_set_context('DRUSH_LOG_CALLBACK', $this->prevLog);
  }

  /**
   * Add a log entry from drush_log.
   *
   * Called by deployotron_log_proxy().
   */
  public function drushLog($entry) {
    $this->log[] = $entry;
  }

  /**
   * Empty the log.
   */
  public function resetLog() {
    $this->log[] = array();
  }

  /**
   * Assert that the log contains a message maching the regexp.
   */
  public function assertLogMessageRegexp($regexp, $type = NULL) {
    foreach ($this->log as $entry) {
      if (preg_match($regexp, $entry['message']) && (!$type || $type == $entry['type'])) {
        $this->assertTrue(TRUE);
        return;
      }
    }
    $this->fail('Log message not found.');
  }

  /**
   * Assert a number of log entries.
   */
  public function assertLogMessageCount($count) {
    $this->assertEquals($count, count($this->log));
  }

  /**
   * Test incomplete actions.
   */
  public function testIncomplete() {
    $incomplete = new IncompleteAction('@none');
    $this->assertLogMessageRegexp('/Incomplete action, missing short description: /', 'warning');
    $this->assertLogMessageCount(1);

    // Check the description is properly created from the default.
    $description = $incomplete->getDescription();
    $this->assertEquals('Run the incompletely implemented action.', $description);

    // Ensure that run() returns false.
    $this->assertFalse($incomplete->run());

    // Ensure that it's disabled (should be as no short name was given).
    $this->assertFalse($incomplete->enabled());
  }
}

class IncompleteAction extends \Deployotron\Action {
}
