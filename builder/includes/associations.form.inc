<?php

/**
 * @file
 *
 * Handles the display and submission of the associations form, which is used to
 * associate xml forms with a given content model's datastream.
 */

/**
 * Defines the associations form.
 *
 * This form lists all the associations of the given XML form, allowing the
 * user to delete/disable those associations, as well as adding new
 * associations to the given XML form.
 *
 * @param array $form
 *   The drupal form.
 * @param array $form_state
 *   The drupal form state.
 * @param string $form_name
 *   The name of an XML form.
 *
 * @return array
 *   The drupal form definition.
 */
function xml_form_builder_associations_form(array $form, array &$form_state, $form_name) {
  module_load_include('inc', 'xml_form_builder', 'includes/associations');
  form_load_include($form_state, 'inc', 'xml_form_builder', 'includes/associations.form');
  $associations = xml_form_builder_get_associations(array($form_name), array(), array(), FALSE);
  $create_table_rows = function($o) {
    $o['title_field'] = "['" . implode("']['", $o['title_field']) . "']";
    $o['type'] = $o['in_db'] ? 'custom' : 'hook';
    return $o;
  };
  $form += array(
    'list' => array(
      '#type' => 'fieldset',
      '#title' => t('Current associations'),
      '#value' => theme('xml_form_builder_association_table', array(
        'associations' => array_map($create_table_rows, $associations),
      )),
    ),
    'fieldset' => array(
      '#type' => 'fieldset',
      '#title' => t('Add Association'),
      '#collapsible' => TRUE,
      'content_model' => array(
        '#type' => 'textfield',
        '#title' => t('Content Model'),
        '#required' => TRUE,
        '#autocomplete_path' => MENU_XML_FORM_BUILDER_CONTENT_MODEL_AUTOCOMPLETE,
        '#description' => t('The content model to associate with a form. If the content model has no decendents it will not show up in autocomplete.'),
        '#default_value' => isset($form_state['values']['content_model']) ? $form_state['values']['content_model'] : NULL,
      ),
      'dsid' => array(
        '#type' => 'textfield',
        '#description' => t('The datastream ID where the object\'s metadata is stored.'),
        '#title' => t('Metadata Datastream ID'),
        '#required' => TRUE,
        '#default_value' => isset($form_state['values']['dsid']) ? $form_state['values']['dsid'] : NULL,
      ),
      'form_name' => array(
        '#type' => 'value',
        '#title' => t('Form Name'),
        '#value' => $form_name,
      ),
      'title_field' => array(
        '#type' => 'select',
        '#title' => t('Title Field'),
        '#description' => t('The form field for the object\'s label.'),
        '#prefix' => '<div id="ahah-wrapper">',
        '#suffix' => '</div>',
        '#options' => xml_form_builder_get_title_options($form_name),
        '#required' => TRUE,
      ),
      'transform' => array(
        '#type' => 'select',
        '#title' => t('XSL Transform'),
        '#options' => xml_form_builder_get_transforms(),
        '#default_value' => 'No Transform',
        '#description' => t("An XSL transform for setting the Fedora object's Dublin Core metadata datastream."),
      ),
      'self_transform' => array(
        '#type' => 'select',
        '#title' => t('Self XSL Transform'),
        '#options' => xml_form_builder_get_self_transforms(),
        '#default_value' => 'No Transform',
        '#description' => t('An optional transformation applied to form data prior to ingestion.'),
      ),
      'file' => array(
        '#type' => 'file',
        '#title' => t('Upload Template Document'),
        '#description' => t('A sample metadata file used to prepopulate the form on ingest.'),
      ),
      'submit' => array(
        '#type' => 'submit',
        '#value' => t('Add Association'),
      ),
    ),
  );
  return $form;
}

/**
 * Default submit handler for the associations form.
 *
 * It adds a new association to the given form.
 *
 * @param array $form
 *   The drupal form.
 * @param array $form_state
 *   The drupal form state.
 */
function xml_form_builder_associations_form_submit(array $form, array &$form_state) {
  $object_keys = array(
    'content_model',
    'form_name',
    'dsid',
    'title_field',
    'transform',
    'self_transform',
  );
  $object = array_intersect_key(
    $form_state['values'],
    array_combine($object_keys, $object_keys)
  );
  $object['template'] = '';
  $file_uploaded = $_FILES['files']['error']['file'] == 0;
  if ($file_uploaded) {
    $definition = new DOMDocument();
    $definition->load($_FILES['files']['tmp_name']['file']);
    $object['template'] = $definition->saveXML();
  }
  if (drupal_write_record('xml_form_builder_form_associations', $object) !== FALSE) {
    drupal_set_message(t('Successfully added association.'));
  }
  else {
    drupal_set_message(t('Failed to add association.'), 'error');
  }
}

/**
 * Get an array of "array paths" from which an object label may be extracted.
 *
 * @param string $form_name
 *   The name of the form in question.
 *
 * @return array
 *   An array of options from which a title may be extracted.
 */
function xml_form_builder_get_title_options($form_name) {
  if (isset($form_name)) {
    module_load_include('inc', 'xml_form_builder', 'XMLFormRepository');
    $doc = XMLFormRepository::Get($form_name);
    if ($doc) {
      $xpath = new DOMXPath($doc);
      $result = $xpath->query('//element[@name]');
      $elements = dom_node_list_to_array($result);
      if (count($elements) > 0) {
        $output = array();
        foreach ($elements as $element) {
          $result = $xpath->query('ancestor-or-self::node()[@name]', $element);
          $ancestors = dom_node_list_to_array($result);
          $ancestors_array = array();
          if (count($ancestors) > 0) {
            foreach ($ancestors as $ancestor) {
              $name = $ancestor->getAttribute('name');
              $ancestors_array[] = (string) $name;
            }
          }
          $output[serialize($ancestors_array)] = "['" . implode("']['", $ancestors_array) . "']";
        }
        return $output;
      }
    }
  }
  return array(NULL => 'invalid');
}

/**
 * Remove the association identified by $id.
 *
 * Either by deleting it from the database, or marking it disabled if its
 * provided by a hook.
 *
 * @param string $form_name
 *   The name of the form for which the associations are being adjusted.
 *   (used to redirect).
 * @param string|int $id
 *   The identifier for the form association.  A string for "default" forms
 *   (added in via associations), and an integer for associations added via
 *   the form.
 */
function xml_form_builder_disable_association($form_name, $id) {
  module_load_include('inc', 'xml_form_builder', 'includes/associations');
  $association = xml_form_builder_get_association($id);
  if (!isset($association)) {
    drupal_set_message(t('Specified association does not exist.'), 'error');
    drupal_goto(xml_form_builder_get_associate_form_path($form_name));
    return;
  }
  // Database defined association.
  if ($association['in_db']) {
    db_delete('xml_form_builder_form_associations')
      ->condition('id', intval($id))
      ->execute();
    drupal_set_message(t('Deleted the association ID:%id from %form_name.', array(
      '%id' => $id,
      '%form_name' => $form_name,
    )));
  }
  else {
    // Hook defined association.
    $num_results = db_select('xml_form_builder_association_hooks', 'fa')
      ->fields('fa')
      ->condition('id', $id)
      ->countQuery()
      ->execute()
      ->fetchField();
    if ($num_results == 1) {
      db_update('xml_form_builder_association_hooks')
        ->fields(array('enabled' => (int) FALSE))
        ->condition('id', $id)
        ->execute();
    }
    else {
      db_insert('xml_form_builder_association_hooks')
        ->fields(array(
          'id' => $id,
          'enabled' => (int) FALSE,
        ))
        ->execute();
    }
    drupal_set_message(t('Successfully disabled association.'));
  }
  drupal_goto(xml_form_builder_get_associate_form_path($form_name));
}

/**
 * Enable a default association identified by $id.
 *
 * @param string $form_name
 *   The name of the form for which the associations are being adjusted.
 *   (used to redirect).
 * @param string $id
 *   The identifier for the form association. Note that only "default"
 *   associations added via hook_xml_form_builder_form_associations() can be
 *   enabled.
 */
function xml_form_builder_enable_association($form_name, $id) {
  module_load_include('inc', 'xml_form_builder', 'includes/associations');
  $association = xml_form_builder_get_association($id);
  if (!isset($association)) {
    drupal_set_message(t('Specified association does not exist.'), 'error');
    drupal_goto(xml_form_builder_get_associate_form_path($form_name));
    return;
  }
  // Hook defined association, can't enable non hook associations.
  if (!$association['in_db']) {
    $num_results = db_select('xml_form_builder_association_hooks', 'fa')
      ->fields('fa')
      ->condition('id', $id)
      ->countQuery()
      ->execute()
      ->fetchField();
    if ($num_results == 1) {
      db_update('xml_form_builder_association_hooks')
        ->fields(array('enabled' => (int) TRUE))
        ->condition('id', $id)
        ->execute();
    }
    else {
      db_insert('xml_form_builder_association_hooks')
          ->fields(array(
            'id' => $id,
            'enabled' => (int) TRUE,
          ))
          ->execute();
    }
  }
  drupal_set_message(t('Successfully enabled association.'));
  drupal_goto(xml_form_builder_get_associate_form_path($form_name));
}
