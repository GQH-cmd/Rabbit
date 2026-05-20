from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from typing import Optional, List
from sqlalchemy import create_engine, Column, Integer, String, Enum, Date, ForeignKey
from sqlalchemy.orm import sessionmaker, declarative_base, relationship

DATABASE_URL = "mysql+pymysql://user:password@localhost:3306/rabbit_farm"

engine = create_engine(DATABASE_URL)
SessionLocal = sessionmaker(bind=engine)
Base = declarative_base()

# =======================
# 数据库模型
# =======================
class Rabbit(Base):
    __tablename__ = 'rabbits'
    id = Column(Integer, primary_key=True, index=True)
    ear_tag = Column(String(50), unique=True, nullable=False)
    name = Column(String(50))
    gender = Column(Enum('M','F'))
    birth_date = Column(Date)

    pedigree = relationship("Pedigree", back_populates="rabbit", uselist=False)

class Pedigree(Base):
    __tablename__ = 'pedigree'
    rabbit_id = Column(Integer, ForeignKey('rabbits.id'), primary_key=True)
    father_id = Column(Integer, ForeignKey('rabbits.id'), nullable=True)
    mother_id = Column(Integer, ForeignKey('rabbits.id'), nullable=True)

    rabbit = relationship("Rabbit", foreign_keys=[rabbit_id], back_populates="pedigree")
    father = relationship("Rabbit", foreign_keys=[father_id])
    mother = relationship("Rabbit", foreign_keys=[mother_id])

Base.metadata.create_all(bind=engine)

# =======================
# Pydantic 模型
# =======================
class RabbitCreate(BaseModel):
    ear_tag: str
    name: Optional[str]
    gender: Optional[str]
    birth_date: Optional[str]
    father_id: Optional[int]
    mother_id: Optional[int]

class RabbitResponse(BaseModel):
    id: int
    ear_tag: str
    name: Optional[str]
    gender: Optional[str]
    birth_date: Optional[str]
    father_id: Optional[int]
    mother_id: Optional[int]

    class Config:
        orm_mode = True

# =======================
# FastAPI 实例
# =======================
app = FastAPI()

# 创建兔子并记录谱系
@app.post("/rabbit", response_model=RabbitResponse)
def create_rabbit(rabbit: RabbitCreate):
    db = SessionLocal()
    existing = db.query(Rabbit).filter(Rabbit.ear_tag == rabbit.ear_tag).first()
    if existing:
        db.close()
        raise HTTPException(status_code=400, detail="耳标已存在")

    new_rabbit = Rabbit(
        ear_tag=rabbit.ear_tag,
        name=rabbit.name,
        gender=rabbit.gender,
        birth_date=rabbit.birth_date
    )
    db.add(new_rabbit)
    db.commit()
    db.refresh(new_rabbit)

    # 插入谱系
    pedigree = Pedigree(
        rabbit_id=new_rabbit.id,
        father_id=rabbit.father_id,
        mother_id=rabbit.mother_id
    )
    db.add(pedigree)
    db.commit()
    db.refresh(pedigree)
    db.close()
    return RabbitResponse(
        id=new_rabbit.id,
        ear_tag=new_rabbit.ear_tag,
        name=new_rabbit.name,
        gender=new_rabbit.gender,
        birth_date=new_rabbit.birth_date,
        father_id=pedigree.father_id,
        mother_id=pedigree.mother_id
    )

# 查询兔子信息
@app.get("/rabbit/{rabbit_id}", response_model=RabbitResponse)
def get_rabbit(rabbit_id: int):
    db = SessionLocal()
    rabbit = db.query(Rabbit).filter(Rabbit.id == rabbit_id).first()
    if not rabbit:
        db.close()
        raise HTTPException(status_code=404, detail="兔子不存在")
    pedigree = rabbit.pedigree
    db.close()
    return RabbitResponse(
        id=rabbit.id,
        ear_tag=rabbit.ear_tag,
        name=rabbit.name,
        gender=rabbit.gender,
        birth_date=rabbit.birth_date,
        father_id=pedigree.father_id if pedigree else None,
        mother_id=pedigree.mother_id if pedigree else None
    )