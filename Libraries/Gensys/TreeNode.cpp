#include "TreeNode.h"

using namespace Gensys;

TreeNode::TreeNode()
{
	m_up = 0;
	m_left = 0;
	m_right = 0;
	m_down = 0;
}

TreeNode::TreeNode(TreeNode* parent)
{
	if (parent->m_down) {
		m_left = parent->m_down;
		while (m_left->m_right) {
			m_left = m_left->m_right;
		}
		m_left->m_right = this;
	}
	else {
		parent->m_down = this;
		m_left = 0;
	}
	m_up = parent;
	m_right = 0;
	m_down = 0;
}

TreeNode::~TreeNode()
{
	while (m_down) {
		delete m_down;
	}
	if (m_left) {
		m_left->m_right = m_right;
	}
	if (m_right) {
		m_right->m_left = m_left;
	}
	if (m_up && m_up->m_down == this) {
		m_up->m_down = m_right;
	}
}
