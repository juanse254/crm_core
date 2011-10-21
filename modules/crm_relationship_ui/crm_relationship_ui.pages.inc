<?php

/**
 * Return a list of relatinships for this contact.
 *
 * @param $crm_contact
 *   Entity object of crm_contact type.
 */
function crm_relationship_ui_contact_relationships($crm_contact) {
  return "This is a stub";
}

/**
 * Return a list of links to add relationship for specific CRM contact.
 *
 * @param $crm_contact
 *   Entity object of crm_contact type.
 */
function crm_relationship_ui_add_relationship($crm_contact) {
  module_load_include('inc', 'crm_relationship_ui');
  
  $ret = '';
  
  foreach (array(0, 1) as $reverse) {    
    $relationship_types = crm_relationship_ui_load_relationship_types($crm_contact->type, $reverse);  
    $relationship_type_links = array();

    foreach ($relationship_types as $relationship_type) {
      $relationship_type_links[$relationship_type->relation_type] = array(
        'href' => 'crm/contact/' . $crm_contact->contact_id . '/relationships/add/' . $relationship_type->relation_type . '/' . $reverse, 
        'title' => $reverse ? $relationship_type->reverse_label : $relationship_type->label,
      );
    }
    
    if (!empty($relationship_type_links)) {
      $heading = $reverse ? t('Reverse relationship types') : t('Relationship types');
      $ret .= theme('links', array(
        'links' => $relationship_type_links,
        'heading' => array('text' => $heading, 'level' => 'h2'),
      ));
    }
  }

  return $ret;
}

/**
 * Generate add relationship form.
 *
 * @param $crm_contact
 *   Entity object of crm_contact type.
 * @param $relationship_type
 *   Relation type object.
 * @param $reverse
 *   Whether relation is reversed.
 */
function crm_relationship_ui_add_relationship_form($form, &$form_state, $crm_contact, $relationship_type, $reverse) {
  module_load_include('inc', 'crm_relationship_ui');

  $form = array();

  $form['relationship_type'] = array(
    '#type' => 'value',
    '#value' => $relationship_type,
  );
  $form['reverse'] = array(
    '#type' => 'value',
    '#value' => $reverse,
  );
  $form['relationship_type_label'] = array(
    '#type' => 'item',
    '#title' => t('Relationship type') . ': ' . $relationship_type->label,
    '#value' => "",
  );
  
  foreach (array(0, 1) as $reverse_field) {
    $contact_type_names = array();
    foreach (crm_relationship_ui_load_contact_types($relationship_type, $reverse_field) as $contact_type) {
      $contact_type_names[] = t($contact_type->name);
    }
    $contact_type_names = implode(', ', $contact_type_names);

    $form[$reverse_field ? 'destination_contact' : 'source_contact'] = array(
      '#title' => $reverse_field ? t('Destination contact name') : t('Source contact name'),
      '#description' => (t('Following contact types allowed: !types.', array('!types' => $contact_type_names))),
      '#type' => 'textfield',
      '#default_value' => !($reverse_field ^ $reverse) ? crm_contact_title($crm_contact) . " [cid:$crm_contact->contact_id]" : '',
      '#disabled' => !($reverse_field ^ $reverse),
      '#required' => true,
      '#autocomplete_path' => 'crm/contact/' . $crm_contact->contact_id . '/relationships/add/' . $relationship_type->relation_type . '/' . $reverse_field . '/autocomplete',
    );
  }    

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add relationship'),
  );
  
  return $form;
}

/**
 * Perform validation for add relationship form.
 */
function crm_relationship_ui_add_relationship_form_validate($form, &$form_state) {
  $relationship_type = $form_state['values']['relationship_type'];
  $reverse = $form_state['values']['reverse'];
  $source_contact = _crm_relationship_ui_get_contact_from_autocomplete_field_value($form_state['values']['source_contact']);
  $destination_contact = _crm_relationship_ui_get_contact_from_autocomplete_field_value($form_state['values']['destination_contact']);

  if (!$relationship_type || !is_object($relationship_type)) {
    form_set_error('relationship_type', t('Relation type is required.'));
    return;
  }

  if (!$source_contact) {
    form_set_error('source_contact', t('Please, input source contact.'));
    return;
  }
  
  if (!$destination_contact) {
    form_set_error('destination_contact', t('Please, input destination contact.'));
    return;
  }

  $source_bundles = array_keys(crm_relationship_ui_load_contact_types($relationship_type, 0));
  if (!in_array($source_contact->type, $source_bundles)) {
   form_set_error('source_contact', t('Please, input source contact of proper type.'));
   return;
  }

  $target_bundles = array_keys(crm_relationship_ui_load_contact_types($relationship_type, $relationship_type->directional));
  if (!in_array($destination_contact->type, $target_bundles)) {
   form_set_error('destination_contact', t('Please, input destination contact of proper type.'));
   return;
  }

  if ($relationship_type->r_unique) {
    if(relation_relation_exists(
      array(
        array('entity_type' => 'crm_contact', 'entity_id' => $source_contact->contact_id),
        array('entity_type' => 'crm_contact', 'entity_id' => $destination_contact->contact_id),
      ),
      $relationship_type->relation_type
    )) {
      form_set_error($reverse ? 'source_contact' : 'destination_contact', t('This relationship exists. It should be unique.'));
      return;
    }
  }
}

/**
 * Perform submit for add relationship form. Save relationship.
 */
function crm_relationship_ui_add_relationship_form_submit($form, &$form_state) {
  $relationship_type = $form_state['values']['relationship_type'];
  $reverse = $form_state['values']['reverse'];
  $source_contact = _crm_relationship_ui_get_contact_from_autocomplete_field_value($form_state['values']['source_contact']);
  $destination_contact = _crm_relationship_ui_get_contact_from_autocomplete_field_value($form_state['values']['destination_contact']);

  // Create relationship between the order and the node
  $endpoints = array(
    0 => array(
      'entity_type' => 'crm_contact',
      'entity_id' => $source_contact->contact_id,
      'entity_bundle' => $source_contact->type,
      'r_index' => 0,
      'entity_key' => $source_contact->type . ': ' . $source_contact->contact_id
    ),
    1 => array(
      'entity_type' => 'crm_contact',
      'entity_id' => $destination_contact->contact_id,
      'entity_bundle' => $destination_contact->type,
      'r_index' => 1,
      'entity_key' => $destination_contact->type . ': ' . $destination_contact->contact_id
    ),
  );
  $relation = relation_create($relationship_type->relation_type, $endpoints);
  $rid = relation_save($relation);

  $redirect_contact = $reverse ? $destination_contact : $source_contact;
  $form_state['redirect'] = 'crm/contact/' . $redirect_contact->contact_id . '/relationships/';
}

/**
 * Autocomplete function for add relationship form. Lookup for contacts.
 * 
 * @param $crm_contact
 *   Entity object of crm_contact type.
 * @param $relationship_type
 *   Relation type object.
 * @param $reverse
 *   Whether relation is reversed.
 * @param $string
 *   String CRM contact is started with.
 */
function crm_relationship_ui_add_relationship_autocomplete($crm_contact, $relationship_type, $reverse, $string) {
  module_load_include('inc', 'crm_relationship_ui');

  $matches = array();
  
  $contact_types = array_keys(crm_relationship_ui_load_contact_types($relationship_type, $reverse));
  
  $query = new EntityFieldQuery;
  $result = $query
    ->entityCondition('entity_type', 'crm_contact')
    ->propertyCondition('contact_id', $crm_contact->contact_id, '<>')
    ->propertyCondition('type', $contact_types, 'IN')
    //TODO: add support to search by name
    //->fieldCondition('contact_name', 'value', $string, 'STARTS_WITH')
    ->execute();

  $contacts = entity_load('crm_contact', array_keys($result['crm_contact']));
  foreach ($contacts as $contact) {
    $matches[crm_contact_title($contact) . " [cid:$contact->contact_id]"] = crm_contact_title($contact);
  }
  
  // return for JS
  drupal_json_output($matches);
}

/**
 * Extract contact id from the value of autocomplete contact field.
 *
 * @param $string
 *   String that is processed.
 */
function _crm_relationship_ui_get_contact_from_autocomplete_field_value($string) {
  $matches = array();
  preg_match('/\[cid:([0-9]+)\]/', $string, $matches);
  if (!array_key_exists(1, $matches) || !is_numeric($matches[1])) {
    return false;
  }
  $contacts = entity_load('crm_contact', array($matches[1]));
  if (empty($contacts)) {
    return false;
  }
  return $contacts[$matches[1]];
}