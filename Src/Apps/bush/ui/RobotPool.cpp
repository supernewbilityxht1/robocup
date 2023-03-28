#include "RobotPool.h"
#include "models/Robot.h"
#include "models/Team.h"
#include "ui/RobotView.h"
#include "ui/TeamSelector.h"
#include "Session.h"

#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QString>
#include <map>
#include <vector>

RobotPoolDelegate::RobotPoolDelegate(RobotPool* pool) :
  QStyledItemDelegate(pool),
  robotPool(pool)
{}

void RobotPoolDelegate::paint(QPainter* painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
  painter->save();
  RobotView* robotView = robotPool->robotViews[index.data().toString()];
  robotView->show();
  robotView->setGeometry(option.rect);
  robotView->render(painter, option.rect.topLeft(), option.rect);
  painter->restore();
}

QSize RobotPoolDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const
{
  RobotView* robotView = robotPool->robotViews[index.data().toString()];
  return robotView->sizeHint();
}

RobotPool::RobotPool(TeamSelector* teamSelector) :
  teamSelector(teamSelector)
{
  setItemDelegate(new RobotPoolDelegate(this));
  setDefaultDropAction(Qt::MoveAction);
  setDragDropMode(QAbstractItemView::DragDrop);
  setAcceptDrops(true);
}

void RobotPool::update()
{
  // delete the old sender widget
  if(toBeDeletedLater)
  {
    toBeDeletedLater->deleteLater();
    toBeDeletedLater = nullptr;
  }

  /* Use the name of the robot which the robot view represents to discover
   * the sender widget since the address of the sender never matches the
   * addresses of the views.
   */
  QString senderName;
  if(sender() && sender()->inherits("RobotView"))
  {
    RobotView* source = dynamic_cast<RobotView*>(sender());
    senderName = source->getRobotName();
  }
  clear();
  for(RobotView* view : robotViews)
  {
    disconnect(view, 0, 0, 0);
    /* Set parent to parentWidget since qt seems to have problems to get the
     * assigned thread if the qobject has no parent.
     */
    view->setParent(parentWidget());
    // do not delete the sender which is needed by postEvent
    if(senderName != view->getRobotName())
      view->deleteLater();
    else
      toBeDeletedLater = view;
  }
  robotViews.clear();
  std::map<std::string, Robot*> robots = Session::getInstance().robotsByName;
  Team* team = teamSelector->getSelectedTeam();
  if(team)
  {
    std::vector<Robot*> robotsInTeam = team->getPlayers();
    for(size_t i = 0; i < robotsInTeam.size(); ++i)
      if(robotsInTeam[i])
        robots.erase(robotsInTeam[i]->name);
  }
  for(std::map<std::string, Robot*>::const_iterator i = robots.begin();
      i != robots.end();
      ++i)
  {
    addItem(QString::fromStdString(i->first));
    RobotView* view = new RobotView(teamSelector, i->second);
    view->setParent(viewport());
    view->hide();
    connect(view, &RobotView::robotChanged, this, &RobotPool::update, Qt::DirectConnection);
    robotViews[QString::fromStdString(i->first)] = view;
  }
}

void RobotPool::mouseMoveEvent(QMouseEvent* me)
{
  if(selectedItems().empty())
    return;
  QDrag* d = new QDrag(this);
  d->setHotSpot(me->pos());
  QMimeData* data = new QMimeData();
  data->setText(currentItem()->text());
  d->setMimeData(data);
  d->exec(Qt::MoveAction);
  me->accept();
}

void RobotPool::dragEnterEvent(QDragEnterEvent* e)
{
  if(e->source() && (e->source()->inherits("RobotView") || e->source()->inherits("RobotPool")))
    e->acceptProposedAction();
}

void RobotPool::dragMoveEvent(QDragMoveEvent* e)
{
  if(e->source() && (e->source()->inherits("RobotView") || e->source()->inherits("RobotPool")))
    e->acceptProposedAction();
}

void RobotPool::dropEvent(QDropEvent* e)
{
  e->accept();
  if(e->source()->inherits("RobotView"))
  {
    //TODO: this is a very simple implementation which drops the order of the list
    RobotView* source = dynamic_cast<RobotView*>(e->source());
    if(!source->getPlayerNumber())
      return;
    source->setSelected(false);
    source->setRobot(nullptr);
    source->update();
    update();
  }
  else // enable move inside the list
    QListWidget::dropEvent(e);
}

void RobotPool::paintEvent(QPaintEvent* e)
{
  QPalette palette = teamSelector->palette();
  palette.setColor(QPalette::Base, palette.color(QPalette::Window));
  setPalette(palette);
  QListWidget::paintEvent(e);
}
